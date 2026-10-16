import type { EnvironmentalVariable, HistoricalReading, HistoryQuery } from '../api/types.ts'
import { environmentalVariables } from '../environmentalVariables.ts'

export type HistoryRange = Pick<HistoryQuery, 'desde' | 'hasta'>
export interface HistorySeries {
  readings: HistoricalReading[]
  loading: boolean
  error: string | null
}
export type HistoricalReadingsState = Record<EnvironmentalVariable, HistorySeries>
type LoadReadings = (query: HistoryQuery, options: { signal: AbortSignal }) => Promise<HistoricalReading[]>

export function lastHourRange(now = Date.now()): HistoryRange {
  return { desde: new Date(now - 3_600_000).toISOString(), hasta: new Date(now).toISOString() }
}

export function initialHistoryState(): HistoricalReadingsState {
  return {
    temperatura: { readings: [], loading: true, error: null },
    humedad: { readings: [], loading: true, error: null },
    co2: { readings: [], loading: true, error: null },
  }
}

// Un ciclo por rango, con resultados independientes y cancelación al reemplazarlo.
export function startHistoricalReadingsLoad(
  load: LoadReadings,
  range: HistoryRange,
  onState: (state: HistoricalReadingsState) => void,
) {
  let stopped = false
  let state = initialHistoryState()
  const controllers: AbortController[] = []
  const timeouts: ReturnType<typeof setTimeout>[] = []
  onState(state)

  for (const { variable } of environmentalVariables) {
    const controller = new AbortController()
    controllers.push(controller)
    let settled = false
    const publish = (result: HistorySeries) => {
      if (stopped || settled) return
      settled = true
      clearTimeout(timeout)
      state = { ...state, [variable]: result }
      onState(state)
    }
    const timeout = setTimeout(() => {
      publish({ readings: [], loading: false, error: 'La consulta del histórico tardó demasiado. Intenta consultar de nuevo.' })
      controller.abort()
    }, 10_000)
    timeouts.push(timeout)

    void (async () => {
      try {
        const readings = await load({ variable, desde: range.desde, hasta: range.hasta }, { signal: controller.signal })
        publish({ readings, loading: false, error: null })
      } catch {
        publish({ readings: [], loading: false, error: 'No se pudo cargar el histórico. Comprueba la conexión con la API.' })
      }
    })()
  }

  return () => {
    stopped = true
    timeouts.forEach(clearTimeout)
    controllers.forEach(controller => controller.abort())
  }
}
