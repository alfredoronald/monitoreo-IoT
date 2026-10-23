import type { AirAlert } from './types.ts'

interface AlertsApi {
  getAlerts: (attended: boolean, options: { signal: AbortSignal }) => Promise<AirAlert[]>
  markAlertAttended: (id: string, options: { signal: AbortSignal }) => Promise<AirAlert>
}

export interface PendingAlertsState {
  alerts: AirAlert[]
  loading: boolean
  error: string | null
  attendingIds: string[]
  actionErrors: Record<string, string>
}

export const initialPendingAlertsState: PendingAlertsState = {
  alerts: [], loading: true, error: null, attendingIds: [], actionErrors: {},
}

export function startPendingAlerts(api: AlertsApi, notify: (state: PendingAlertsState) => void) {
  let state = { ...initialPendingAlertsState }
  let stopped = false
  let revision = 0
  let query: AbortController | undefined
  const actions = new Map<string, AbortController>()

  function update(changes: Partial<PendingAlertsState>) {
    if (stopped) return
    state = { ...state, ...changes }
    notify(state)
  }

  async function refresh() {
    if (stopped || query) return
    const controller = new AbortController()
    const version = revision
    query = controller
    const timeout = setTimeout(() => controller.abort(), 10_000)
    try {
      const alerts = await api.getAlerts(false, { signal: controller.signal })
      if (!stopped && version === revision) update({ alerts, loading: false, error: null })
    } catch {
      if (!stopped && version === revision) update({ loading: false, error: 'No se pudieron actualizar las alertas. Se reintentará automáticamente.' })
    } finally {
      clearTimeout(timeout)
      if (query === controller) query = undefined
    }
  }

  async function markAttended(id: string) {
    if (stopped || actions.has(id)) return
    const controller = new AbortController()
    actions.set(id, controller)
    const actionErrors = { ...state.actionErrors }
    delete actionErrors[id]
    update({ attendingIds: [...actions.keys()], actionErrors })
    const timeout = setTimeout(() => controller.abort(), 10_000)
    try {
      await api.markAlertAttended(id, { signal: controller.signal })
      if (stopped) return
      revision++
      query?.abort()
      query = undefined
      update({ alerts: state.alerts.filter(alert => alert.id !== id) })
      void refresh()
    } catch {
      update({ actionErrors: { ...state.actionErrors, [id]: 'No se pudo atender la alerta. Inténtalo de nuevo.' } })
    } finally {
      clearTimeout(timeout)
      actions.delete(id)
      update({ attendingIds: [...actions.keys()] })
    }
  }

  void refresh()
  const interval = setInterval(() => { void refresh() }, 15_000)
  return {
    markAttended,
    stop() {
      stopped = true
      clearInterval(interval)
      query?.abort()
      for (const controller of actions.values()) controller.abort()
    },
  }
}
