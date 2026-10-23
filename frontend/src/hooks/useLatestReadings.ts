import { useEffect, useState } from 'react'
import { api } from '../api/index.ts'
import type { LatestReading } from '../api/types.ts'
import { startLatestReadingsPolling } from '../api/latestReadingsPoller.ts'

export function useLatestReadings() {
  const [state, setState] = useState<{ readings: LatestReading[]; loading: boolean; error: string | null }>({ readings: [], loading: true, error: null })
  useEffect(() => startLatestReadingsPolling(api.getLatestReadings, result => {
    setState(previous => ({
      readings: 'readings' in result ? result.readings : previous.readings,
      loading: false,
      error: result.error,
    }))
  }), [])
  return state
}
