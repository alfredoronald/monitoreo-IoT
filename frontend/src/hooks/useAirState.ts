import { useEffect, useState } from 'react'
import { api } from '../api/index.ts'
import type { AirState } from '../api/types.ts'

const AIR_STATE_INTERVAL_MS = 15_000

export function useAirState() {
  const [state, setState] = useState<{ airState: AirState | null; loading: boolean; error: string | null }>({
    airState: null,
    loading: true,
    error: null,
  })

  useEffect(() => {
    let stopped = false
    let active: AbortController | undefined

    async function refresh() {
      if (stopped || active) return
      const controller = new AbortController()
      active = controller
      const timeout = setTimeout(() => controller.abort(), 10_000)
      try {
        const airState = await api.getAirState({ signal: controller.signal })
        if (!stopped) setState({ airState, loading: false, error: null })
      } catch {
        if (!stopped) setState(previous => ({ ...previous, loading: false, error: 'No se pudo actualizar el estado del aire.' }))
      } finally {
        clearTimeout(timeout)
        if (active === controller) active = undefined
      }
    }

    void refresh()
    const interval = setInterval(() => { void refresh() }, AIR_STATE_INTERVAL_MS)
    return () => {
      stopped = true
      clearInterval(interval)
      active?.abort()
    }
  }, [])

  return state
}
