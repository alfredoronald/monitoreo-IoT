import type { LatestReading } from './types.ts'
import { ApiError } from './client.ts'

export const LATEST_READINGS_INTERVAL_MS = 15_000
type Result = { readings: LatestReading[]; error: null } | { error: string }

// Una petición activa como máximo; al salir se cancela también su timeout.
export function startLatestReadingsPolling(
  load: (options: { signal: AbortSignal }) => Promise<LatestReading[]>,
  onResult: (result: Result) => void,
) {
  let stopped = false
  let active: AbortController | undefined
  async function refresh() {
    if (stopped || active) return
    const controller = new AbortController()
    active = controller
    const timeout = setTimeout(() => controller.abort(), 10_000)
    try {
      const readings = await load({ signal: controller.signal })
      if (!stopped) onResult({ readings, error: null })
    } catch (error) {
      if (!stopped) onResult({ error: error instanceof ApiError && error.status === 401 ? 'No autorizado: revisa la clave de la API.' : 'No se pudieron actualizar las lecturas. Se reintentará automáticamente.' })
    } finally {
      clearTimeout(timeout)
      if (active === controller) active = undefined
    }
  }
  void refresh()
  const interval = setInterval(() => { void refresh() }, LATEST_READINGS_INTERVAL_MS)
  return () => {
    stopped = true
    clearInterval(interval)
    active?.abort()
  }
}
