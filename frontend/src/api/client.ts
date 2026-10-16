import type { AirAlert, AirState, HistoricalReading, HistoryQuery, LatestReading } from './types.ts'

type RequestOptions = Pick<RequestInit, 'signal'>
type Guard<T> = (value: unknown) => value is T
type RecordValue = Record<string, unknown>
const object = (value: unknown): value is RecordValue => typeof value === 'object' && value !== null && !Array.isArray(value)
const id = (value: unknown) => typeof value === 'string' && /^[1-9]\d*$/.test(value) && BigInt(value) <= 9223372036854775807n
const date = (value: unknown) => typeof value === 'string' && /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/.test(value) && Number.isFinite(Date.parse(value)) && new Date(value).toISOString() === value
const reading: Guard<LatestReading> = (value): value is LatestReading => object(value) && typeof value.tipo_variable === 'string' && ['temperatura', 'humedad', 'co2'].includes(value.tipo_variable) && typeof value.valor === 'number' && Number.isFinite(value.valor) && date(value.fecha_hora)
const historical: Guard<HistoricalReading> = (value): value is HistoricalReading => reading(value) && object(value) && id(value.id)
const air: Guard<AirState> = (value): value is AirState => reading(value) && object(value) && value.tipo_variable === 'co2' && typeof value.nivel === 'string' && ['bueno', 'moderado', 'malo'].includes(value.nivel) && typeof value.etiqueta === 'string' && (value.recomendacion === null || typeof value.recomendacion === 'string')
const alert: Guard<AirAlert> = (value): value is AirAlert => object(value) && id(value.id) && id(value.lectura_id) && id(value.umbral_id) && typeof value.mensaje === 'string' && date(value.fecha_hora) && typeof value.atendida === 'boolean'
const list = <T>(guard: Guard<T>): Guard<T[]> => (value): value is T[] => Array.isArray(value) && value.every(guard)

export class ApiError extends Error {
  readonly status: number
  constructor(status: number, message: string) {
    super(message)
    this.name = 'ApiError'
    this.status = status
  }
}

export function createApiClient(baseUrl: string, fetcher: typeof fetch = globalThis.fetch) {
  const base = new URL(baseUrl)
  if (!['http:', 'https:'].includes(base.protocol) || base.search || base.hash || base.username || base.password) {
    throw new TypeError('VITE_API_URL debe ser una URL HTTP sin credenciales, query ni fragmento.')
  }
  const prefix = base.href.replace(/\/+$/, '')

  async function request<T>(path: string, guard: Guard<T>, options: RequestInit = {}): Promise<T> {
    const response = await fetcher(`${prefix}${path}`, { ...options, headers: { Accept: 'application/json', ...options.headers } })
    // Conservar AbortError y los fallos de red para que la interfaz los trate.
    const json: unknown = await response.json().catch((error: unknown) => {
      if (error instanceof SyntaxError) return undefined
      throw error
    })
    if (!response.ok) {
      const message = object(json) && typeof json.mensaje === 'string' ? json.mensaje : `La API respondió con HTTP ${response.status}.`
      throw new ApiError(response.status, message)
    }
    if (!guard(json)) throw new TypeError('La respuesta de la API no coincide con el contrato.')
    return json
  }

  return {
    getReadings(query: HistoryQuery, options: RequestOptions = {}) {
      const search = new URLSearchParams({ variable: query.variable, desde: query.desde, hasta: query.hasta })
      return request(`/lecturas?${search}`, list(historical), options)
    },
    getLatestReadings(options: RequestOptions = {}) {
      return request('/lecturas/ultima', list(reading), options)
    },
    getAirState(options: RequestOptions = {}) {
      return request('/estado-aire', (value): value is AirState | null => value === null || air(value), options)
    },
    getAlerts(attended?: boolean, options: RequestOptions = {}) {
      const search = attended === undefined ? '' : `?atendida=${attended}`
      return request(`/alertas${search}`, list(alert), options)
    },
    markAlertAttended(alertId: string, options: RequestOptions = {}) {
      if (!id(alertId)) throw new TypeError('El identificador de alerta debe ser una cadena BIGINT positiva.')
      return request(`/alertas/${alertId}`, (value): value is AirAlert => alert(value) && value.atendida === true, {
        ...options, method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ atendida: true }),
      })
    },
  }
}
