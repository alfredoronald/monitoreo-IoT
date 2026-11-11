import { useEffect, useRef, useState } from 'react'
import { api } from '../api/index.ts'
import { initialPendingAlertsState, startPendingAlerts } from '../api/pendingAlerts.ts'

export function usePendingAlerts() {
  const [state, setState] = useState(initialPendingAlertsState)
  const controller = useRef<ReturnType<typeof startPendingAlerts> | null>(null)

  useEffect(() => {
    const current = startPendingAlerts(api, setState)
    controller.current = current
    return () => {
      controller.current = null
      current.stop()
    }
  }, [])

  return { ...state, markAttended: (id: string) => { void controller.current?.markAttended(id) } }
}
