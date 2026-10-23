import assert from 'node:assert/strict'
import test from 'node:test'
import { isReadingRecent, startReadingClock } from '../src/readings/readingFreshness.ts'

const timestamp = '2026-10-18T16:00:00.000Z'
const epoch = Date.parse(timestamp)

test('Antigüedad: 59999 y 60000 ms son recientes; 60001 ms es antiguo', () => {
  assert.equal(isReadingRecent(timestamp, epoch + 59_999), true)
  assert.equal(isReadingRecent(timestamp, epoch + 60_000), true)
  assert.equal(isReadingRecent(timestamp, epoch + 60_001), false)
  assert.equal(isReadingRecent('fecha inválida', epoch), false)
})

test('La antigüedad depende solo de fecha_hora y es independiente por variable', () => {
  const readings = [
    { tipo_variable: 'temperatura', valor: 0, fecha_hora: timestamp },
    { tipo_variable: 'humedad', valor: 55, fecha_hora: new Date(epoch + 30_000).toISOString() },
    { tipo_variable: 'co2', valor: 850, fecha_hora: new Date(epoch + 60_000).toISOString() },
  ]
  assert.deepEqual(readings.map(reading => isReadingRecent(reading.fecha_hora, epoch + 60_001)), [false, true, true])
  assert.equal(readings[0].valor, 0)
})

test('El reloj vence cada lectura al superar el minuto sin respuestas HTTP nuevas', t => {
  t.mock.timers.enable({ apis: ['Date', 'setTimeout'], now: epoch })
  const later = new Date(epoch + 15_000).toISOString()
  const states = []
  t.after(startReadingClock([timestamp, later], () => {
    states.push([isReadingRecent(timestamp, Date.now()), isReadingRecent(later, Date.now())])
  }))
  assert.deepEqual(states, [[true, true]])
  t.mock.timers.tick(60_000)
  assert.equal(states.length, 1)
  t.mock.timers.tick(1)
  assert.deepEqual(states.at(-1), [false, true])
  t.mock.timers.tick(15_000)
  assert.deepEqual(states.at(-1), [false, false])
})

test('Reemplazar una muestra recupera la vigencia y desmontar cancela el reloj', t => {
  t.mock.timers.enable({ apis: ['Date', 'setTimeout'], now: epoch + 60_001 })
  const states = []
  const stopOld = startReadingClock([timestamp], () => states.push(isReadingRecent(timestamp, Date.now())))
  assert.deepEqual(states, [false])
  stopOld()
  const fresh = new Date(Date.now()).toISOString()
  const stop = startReadingClock([fresh], () => states.push(isReadingRecent(fresh, Date.now())))
  assert.deepEqual(states, [false, true])
  stop()
  t.mock.timers.tick(120_000)
  assert.deepEqual(states, [false, true])
})

test('Sin muestras o con muestras antiguas el reloj no mantiene temporizadores', t => {
  t.mock.timers.enable({ apis: ['Date', 'setTimeout'], now: epoch + 120_000 })
  let ticks = 0
  t.after(startReadingClock([], () => ticks++))
  t.after(startReadingClock([timestamp], () => ticks++))
  t.mock.timers.tick(300_000)
  assert.equal(ticks, 2)
})
