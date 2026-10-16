import assert from 'node:assert/strict'
import test from 'node:test'
import { startLatestReadingsPolling } from '../src/api/latestReadingsPoller.ts'

const flush = async () => { await Promise.resolve(); await Promise.resolve() }
const sample = [{ tipo_variable: 'temperatura', valor: 0, fecha_hora: '2026-10-15T18:00:00.000Z' }]

test('Consulta inmediatamente y a los 15 segundos, sin recargar y conservando el timestamp recibido', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  let requests = 0
  const results = []
  const stop = startLatestReadingsPolling(async () => { requests++; return sample }, result => results.push(result))
  t.after(stop)
  await flush()
  assert.equal(requests, 1)
  assert.deepEqual(results[0], { readings: sample, error: null })
  t.mock.timers.tick(14_999)
  assert.equal(requests, 1)
  t.mock.timers.tick(1)
  await flush()
  assert.equal(requests, 2)
  stop()
  t.mock.timers.tick(30_000)
  assert.equal(requests, 2)
})

test('Un fallo no reemplaza los datos con ceros y la próxima consulta se recupera', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  let attempts = 0
  const results = []
  const stop = startLatestReadingsPolling(async () => {
    if (++attempts === 2) throw new Error('offline')
    return attempts === 3 ? [] : sample
  }, result => results.push(result))
  t.after(stop)
  await flush()
  t.mock.timers.tick(15_000)
  await flush()
  assert.equal('readings' in results[1], false)
  assert.match(results[1].error, /reintentará/)
  t.mock.timers.tick(15_000)
  await flush()
  assert.deepEqual(results[2], { readings: [], error: null })
})

test('Cancela peticiones lentas, evita superposiciones e ignora respuestas después de salir', async t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  const signals = []
  const results = []
  let resolve
  const stop = startLatestReadingsPolling(({ signal }) => {
    signals.push(signal)
    return new Promise(done => { resolve = done })
  }, result => results.push(result))
  t.after(stop)
  t.mock.timers.tick(10_000)
  assert.equal(signals[0].aborted, true)
  t.mock.timers.tick(5_000)
  assert.equal(signals.length, 1)
  stop()
  resolve(sample)
  await flush()
  assert.deepEqual(results, [])
})

test('Al desmontar cancela también una petición que aún está activa', t => {
  t.mock.timers.enable({ apis: ['setInterval', 'setTimeout'] })
  let requestSignal
  const stop = startLatestReadingsPolling(({ signal }) => { requestSignal = signal; return new Promise(() => {}) }, () => assert.fail('No debe notificar después de salir'))
  stop()
  assert.equal(requestSignal.aborted, true)
})
