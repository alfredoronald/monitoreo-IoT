import assert from 'node:assert/strict'
import test from 'node:test'
import { createApiClient } from '../src/api/client.ts'
import { lastHourRange, startHistoricalReadingsLoad } from '../src/history/historicalReadings.ts'
import { formatHistoryDate, formatHistoryTick, formatHistoryValue, historyPoints } from '../src/history/historyFormatters.ts'

const range = { desde: '2026-10-15T12:00:00.000Z', hasta: '2026-10-15T13:00:00.000Z' }
const sample = [{ id: '9007199254740993', tipo_variable: 'temperatura', valor: 0, fecha_hora: '2026-10-15T12:00:15.123Z' }]
const flush = async () => { for (let i = 0; i < 12; i++) await Promise.resolve() }
function pendingRequests() {
  const requests = []
  const load = (query, { signal }) => new Promise((resolve, reject) => requests.push({ query, signal, resolve, reject }))
  return { requests, load }
}

test('Última hora: extremos ISO explícitos y una duración exacta incluso al cambiar de día', () => {
  const now = Date.parse('2026-10-16T00:30:00.123Z')
  assert.deepEqual(lastHourRange(now), { desde: '2026-10-15T23:30:00.123Z', hasta: '2026-10-16T00:30:00.123Z' })
})

test('Carga tres históricos mediante el cliente real, con el mismo rango, sin limite y sin fabricar lecturas', async t => {
  const calls = []
  const api = createApiClient('http://api.example.test/api', async (url, options) => {
    calls.push({ url: new URL(url), signal: options.signal })
    return Response.json(new URL(url).searchParams.get('variable') === 'temperatura' ? sample : [])
  })
  const states = []
  t.after(startHistoricalReadingsLoad(api.getReadings, range, state => states.push(state)))
  await flush()
  assert.equal(calls.length, 3)
  assert.deepEqual(calls.map(call => call.url.searchParams.get('variable')), ['temperatura', 'humedad', 'co2'])
  for (const { url, signal } of calls) {
    assert.equal(url.pathname, '/api/lecturas')
    assert.equal(url.searchParams.get('desde'), range.desde)
    assert.equal(url.searchParams.get('hasta'), range.hasta)
    assert.deepEqual([...url.searchParams.keys()], ['variable', 'desde', 'hasta'])
    assert.ok(signal instanceof AbortSignal)
  }
  assert.equal(states[0].temperatura.loading, true)
  const last = states.at(-1)
  assert.deepEqual(last.temperatura, { readings: sample, loading: false, error: null })
  assert.deepEqual(last.humedad, { readings: [], loading: false, error: null })
  assert.deepEqual(last.co2, { readings: [], loading: false, error: null })
})

test('Un fallo parcial no oculta los otros gráficos ni confunde un error con una respuesta vacía', async t => {
  const { load, requests } = pendingRequests()
  const states = []
  t.after(startHistoricalReadingsLoad(load, range, state => states.push(state)))
  requests[1].reject(new Error('offline'))
  requests[2].resolve([])
  requests[0].resolve(sample)
  await flush()
  const last = states.at(-1)
  assert.deepEqual(last.temperatura.readings, sample)
  assert.equal(last.humedad.loading, false)
  assert.match(last.humedad.error, /No se pudo cargar/)
  assert.deepEqual(last.humedad.readings, [])
  assert.equal(last.co2.error, null)
  assert.deepEqual(last.co2.readings, [])
  // Las notificaciones anteriores son snapshots, no objetos mutados después.
  assert.equal(states[0].humedad.loading, true)
  assert.equal(states[1].temperatura.loading, true)
})

test('Desmontar aborta las tres solicitudes y descarta respuestas tardías aunque fetch ignore la cancelación', async () => {
  const { load, requests } = pendingRequests()
  const states = []
  const stop = startHistoricalReadingsLoad(load, range, state => states.push(state))
  stop()
  assert.ok(requests.every(request => request.signal.aborted))
  requests[0].resolve(sample)
  requests[1].reject(new DOMException('Aborted', 'AbortError'))
  requests[2].resolve([])
  await flush()
  assert.equal(states.length, 1)
})

test('Cambiar de rango permite resultados nuevos sin sobrescribirlos con los del rango anterior', async t => {
  const { load, requests } = pendingRequests()
  const states = []
  const stop = startHistoricalReadingsLoad(load, range, state => states.push(state))
  stop()
  const nextRange = { desde: '2026-10-14T12:00:00.000Z', hasta: range.hasta }
  t.after(startHistoricalReadingsLoad(load, nextRange, state => states.push(state)))
  assert.ok(requests.slice(0, 3).every(request => request.signal.aborted))
  assert.ok(requests.slice(3).every(request => request.query.desde === nextRange.desde && request.query.hasta === nextRange.hasta))
  requests.slice(3).forEach(request => request.resolve([]))
  await flush()
  const notificationCount = states.length
  requests.slice(0, 3).forEach(request => request.resolve(sample))
  await flush()
  assert.equal(states.length, notificationCount)
  assert.deepEqual(states.at(-1).temperatura.readings, [])
})

test('Una solicitud bloqueada termina en error y no puede publicar datos después del timeout', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const { load, requests } = pendingRequests()
  const states = []
  t.after(startHistoricalReadingsLoad(load, range, state => states.push(state)))
  requests[0].resolve(sample)
  await flush()
  t.mock.timers.tick(10_000)
  assert.equal(requests[0].signal.aborted, false)
  assert.ok(requests.slice(1).every(request => request.signal.aborted))
  assert.deepEqual(states.at(-1).temperatura.readings, sample)
  assert.match(states.at(-1).humedad.error, /tardó demasiado/)
  requests.slice(1).forEach(request => request.resolve([]))
  await flush()
  assert.match(states.at(-1).humedad.error, /tardó demasiado/)
})

test('La cancelación elimina los timeouts pendientes sin emitir errores posteriores', t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const { load } = pendingRequests()
  const states = []
  const stop = startHistoricalReadingsLoad(load, range, state => states.push(state))
  stop()
  t.mock.timers.tick(20_000)
  assert.equal(states.length, 1)
})

test('Los puntos conservan ceros, decimales, BIGINT y timestamps repetidos con distancias temporales reales', () => {
  const readings = [...sample,
    { ...sample[0], id: '9007199254740994', valor: -12.345, fecha_hora: '2026-10-15T12:00:30.123Z' },
    { ...sample[0], id: '9007199254740995', valor: 27.89, fecha_hora: '2026-10-15T12:10:30.123Z' },
    { ...sample[0], id: '9007199254740996' },
  ]
  const points = historyPoints(readings)
  assert.equal(points.length, 4)
  assert.equal(points[1].timestamp - points[0].timestamp, 15_000)
  assert.equal(points[2].timestamp - points[1].timestamp, 600_000)
  assert.equal(points[3].timestamp, points[0].timestamp)
  assert.deepEqual(points.map(({ timestamp: _timestamp, ...reading }) => reading), readings)
  assert.equal('timestamp' in readings[0], false)
})

test('Los formatos muestran fecha y hora en el eje, hora completa con milisegundos y zona en tooltip, y unidades', () => {
  const date = sample[0].fecha_hora
  const local = new Date(date)
  const localHour = String(local.getHours()).padStart(2, '0')
  const localDay = String(local.getDate()).padStart(2, '0')
  const localMonth = String(local.getMonth() + 1).padStart(2, '0')
  const tick = formatHistoryTick(Date.parse(date))
  assert.ok(tick.includes(`${localDay}/${localMonth}`))
  assert.ok(tick.includes(`${localHour}:00`))
  const exact = formatHistoryDate(date)
  assert.match(exact, /2026/)
  assert.ok(exact.includes(`${localHour}:00:15`))
  assert.match(exact, /[.,]123/)
  assert.match(exact, /GMT/)
  assert.match(formatHistoryValue(0, '°C'), /^0 °C$/)
  assert.match(formatHistoryValue(55.25, '% HR'), /^55[.,]25 % HR$/)
  assert.equal(formatHistoryValue(900, 'ppm'), '900 ppm')
})
