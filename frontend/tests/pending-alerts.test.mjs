import assert from 'node:assert/strict'
import test from 'node:test'
import { startPendingAlerts } from '../src/api/pendingAlerts.ts'

const sample = id => ({ id, lectura_id: '1', umbral_id: '2', mensaje: 'CO₂ moderado', fecha_hora: '2026-10-17T12:00:00.000Z', atendida: false })
const flush = async () => { for (let i = 0; i < 8; i++) await Promise.resolve() }
const deferred = () => { let resolve; const promise = new Promise(done => { resolve = done }); return { promise, resolve } }

test('Consulta pendientes al inicio y cada 15 s; atender todas deja la lista vacía', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  let alerts = [sample('1'), sample('2')]
  let queries = 0
  let state
  const controller = startPendingAlerts({
    getAlerts: async attended => { assert.equal(attended, false); queries++; return [...alerts] },
    markAlertAttended: async id => { alerts = alerts.filter(a => a.id !== id); return { ...sample(id), atendida: true } },
  }, next => { state = next })
  t.after(controller.stop)
  await flush()
  assert.equal(state.alerts.length, 2)
  await controller.markAttended('1')
  await flush()
  assert.deepEqual(state.alerts.map(a => a.id), ['2'])
  await controller.markAttended('2')
  await flush()
  assert.deepEqual(state.alerts, [])
  alerts = [sample('3')]
  t.mock.timers.tick(15_000)
  await flush()
  assert.equal(state.alerts[0].id, '3')
  assert.equal(queries, 4)
})

test('Un PATCH fallido conserva la alerta y permite reintentar; evita doble clic', async t => {
  let state
  let calls = 0
  const pending = deferred()
  const controller = startPendingAlerts({
    getAlerts: async () => [sample('1')],
    markAlertAttended: async () => { calls++; await pending.promise; throw new Error('offline') },
  }, next => { state = next })
  t.after(controller.stop)
  await flush()
  const action = controller.markAttended('1')
  await controller.markAttended('1')
  assert.equal(calls, 1)
  assert.deepEqual(state.attendingIds, ['1'])
  pending.resolve()
  await action
  assert.equal(state.alerts.length, 1)
  assert.match(state.actionErrors['1'], /Inténtalo/)
  assert.deepEqual(state.attendingIds, [])
  await controller.markAttended('1')
  assert.equal(calls, 2)
})

test('Una consulta antigua no vuelve a insertar una alerta atendida', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  const stale = deferred()
  let queries = 0
  let state
  const controller = startPendingAlerts({
    getAlerts: async () => { queries++; return queries === 1 ? [sample('1')] : queries === 2 ? stale.promise : [] },
    markAlertAttended: async () => ({ ...sample('1'), atendida: true }),
  }, next => { state = next })
  t.after(controller.stop)
  await flush()
  t.mock.timers.tick(15_000)
  await controller.markAttended('1')
  await flush()
  stale.resolve([sample('1')])
  await flush()
  assert.deepEqual(state.alerts, [])
})

test('Un fallo de consulta mantiene los avisos y desmontar cancela acciones sin notificar', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  let calls = 0
  let state
  let signal
  const pending = deferred()
  let notifications = 0
  const controller = startPendingAlerts({
    getAlerts: async () => { if (++calls > 1) throw new Error('offline'); return [sample('1')] },
    markAlertAttended: async (_, options) => { signal = options.signal; return pending.promise },
  }, next => { state = next; notifications++ })
  t.after(controller.stop)
  await flush()
  t.mock.timers.tick(15_000)
  await flush()
  assert.equal(state.alerts.length, 1)
  assert.match(state.error, /actualizar/)
  const action = controller.markAttended('1')
  controller.stop()
  assert.equal(signal.aborted, true)
  const previous = notifications
  pending.resolve({ ...sample('1'), atendida: true })
  await action
  assert.equal(notifications, previous)
})
