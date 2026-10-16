import assert from 'node:assert/strict'
import test from 'node:test'
import { ApiError, createApiClient } from '../src/api/client.ts'

const timestamp = '2026-10-15T12:00:15.000Z'
const latest = { tipo_variable: 'temperatura', valor: 24.7, fecha_hora: timestamp }
const history = { ...latest, id: '9223372036854775807' }
const alert = { id: '7', lectura_id: '102', umbral_id: '2', mensaje: 'Ventilar', fecha_hora: timestamp, atendida: false }
const air = { tipo_variable: 'co2', valor: 900, fecha_hora: timestamp, nivel: 'moderado', etiqueta: 'Aire moderado', recomendacion: null }
const query = { variable: 'temperatura', desde: '2026-10-15T08:00:00+02:00', hasta: '2026-10-15T09:00:00+02:00' }
function fixture(body, status = 200) {
  const calls = []
  const client = createApiClient('http://example.test:4000/api/', async (url, options) => {
    calls.push({ url: new URL(url), options })
    return Response.json(body, { status })
  })
  return { client, calls }
}

test('Histórico: codifica fechas con +, conserva BIGINT y no agrega limite', async () => {
  const { client, calls } = fixture([history])
  assert.deepEqual(await client.getReadings(query), [history])
  assert.equal(calls[0].url.pathname, '/api/lecturas')
  assert.equal(calls[0].url.searchParams.get('desde'), query.desde)
  assert.equal(calls[0].url.searchParams.get('hasta'), query.hasta)
  assert.equal(calls[0].url.searchParams.get('variable'), 'temperatura')
  assert.match(calls[0].url.search, /%2B/)
  assert.equal(calls[0].url.searchParams.has('limite'), false)
  assert.equal(calls[0].options.body, undefined)
})

test('Últimas: array directo sin exigir id y sin inventar variables', async () => {
  const { client, calls } = fixture([latest])
  assert.deepEqual(await client.getLatestReadings(), [latest])
  assert.equal(calls[0].url.pathname, '/api/lecturas/ultima')
  assert.equal(calls[0].url.search, '')
  assert.equal(calls[0].options.headers['Content-Type'], undefined)
})

test('Estado de aire: objeto singular y recomendación nullable', async () => {
  const { client, calls } = fixture(air)
  assert.deepEqual(await client.getAirState(), air)
  assert.equal(calls[0].url.pathname, '/api/estado-aire')
  assert.equal(await fixture(null).client.getAirState(), null)
})

test('Listas vacías son resultados válidos en las tres consultas de listas', async () => {
  const { client } = fixture([])
  assert.deepEqual(await client.getReadings(query), [])
  assert.deepEqual(await client.getLatestReadings(), [])
  assert.deepEqual(await client.getAlerts(false), [])
})

test('Alertas: false, true y filtro omitido se distinguen', async () => {
  const { client, calls } = fixture([alert])
  assert.deepEqual(await client.getAlerts(false), [alert])
  await client.getAlerts(true)
  await client.getAlerts()
  assert.equal(calls[0].url.search, '?atendida=false')
  assert.equal(calls[1].url.search, '?atendida=true')
  assert.equal(calls[2].url.search, '')
})

test('PATCH: ruta, JSON true y Content-Type correctos; ID nunca convertido a Number', async () => {
  const { client, calls } = fixture({ ...alert, atendida: true })
  assert.equal((await client.markAlertAttended('9223372036854775807')).atendida, true)
  assert.equal(calls[0].url.pathname, '/api/alertas/9223372036854775807')
  assert.equal(calls[0].options.method, 'PATCH')
  assert.equal(calls[0].options.headers['Content-Type'], 'application/json')
  assert.deepEqual(JSON.parse(calls[0].options.body), { atendida: true })
})

test('IDs inválidos no generan solicitudes PATCH', () => {
  const { client, calls } = fixture(alert)
  for (const value of ['0', '-1', '1/extra', '1.5', '9223372036854775808']) {
    assert.throws(() => client.markAlertAttended(value), TypeError)
  }
  assert.equal(calls.length, 0)
})

test('HTTP 400, 404, 415 y 500 conservan status y mensaje del contrato', async () => {
  for (const status of [400, 404, 415, 500]) {
    const { client } = fixture({ mensaje: 'Solicitud rechazada' }, status)
    await assert.rejects(client.getLatestReadings(), error => error instanceof ApiError && error.status === status && error.message === 'Solicitud rechazada')
  }
})

test('Error HTTP sin JSON usa mensaje genérico; éxito sin JSON no se acepta', async () => {
  for (const status of [502, 200]) {
    const client = createApiClient('http://example.test/api', async () => new Response('<html>Error</html>', { status }))
    await assert.rejects(client.getAirState(), status === 502 ? error => error instanceof ApiError && error.status === 502 : TypeError)
  }
})

test('Rechaza NUMERIC textual, JSON envuelto, fechas imposibles y variables desconocidas', async () => {
  for (const body of [[{ ...latest, valor: '24.7' }], { data: [latest] }, [{ ...latest, fecha_hora: '2026-02-31T00:00:00.000Z' }], [{ ...latest, tipo_variable: 'ruido' }], [{ ...latest, tipo_variable: ['temperatura'] }]]) {
    await assert.rejects(fixture(body).client.getLatestReadings(), TypeError)
  }
})

test('Rechaza alertas mal tipadas y PATCH sin confirmación atendida', async () => {
  await assert.rejects(fixture([{ ...alert, id: 7 }]).client.getAlerts(), TypeError)
  await assert.rejects(fixture(alert).client.markAlertAttended('7'), TypeError)
})

test('URL configurable: se preserva el prefijo y se rechazan bases inseguras o inválidas', () => {
  for (const value of ['', '/api', 'file:///api', 'http://user:pass@example.test/api', 'http://example.test/api?x=1']) {
    assert.throws(() => createApiClient(value), TypeError)
  }
})

test('Cada operación propaga AbortSignal', async () => {
  const controller = new AbortController()
  const cases = [[[], api => api.getReadings(query, { signal: controller.signal })], [[], api => api.getLatestReadings({ signal: controller.signal })], [null, api => api.getAirState({ signal: controller.signal })], [[], api => api.getAlerts(false, { signal: controller.signal })], [{ ...alert, atendida: true }, api => api.markAlertAttended('7', { signal: controller.signal })]]
  for (const [body, invoke] of cases) {
    const { client, calls } = fixture(body)
    await invoke(client)
    assert.equal(calls[0].options.signal, controller.signal)
  }
})

test('Fallo de red y cancelación no se disfrazan de datos vacíos ni errores HTTP', async () => {
  for (const error of [new TypeError('Failed to fetch'), new DOMException('Cancelado', 'AbortError')]) {
    const client = createApiClient('http://example.test/api', async () => { throw error })
    await assert.rejects(client.getLatestReadings(), actual => actual === error)
  }
})
