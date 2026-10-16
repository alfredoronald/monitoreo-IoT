import { useEffect, useState } from 'react'
import { api } from '../api/index.ts'
import { initialHistoryState, startHistoricalReadingsLoad } from '../history/historicalReadings.ts'
import type { HistoryRange } from '../history/historicalReadings.ts'

export function useHistoricalReadings({ desde, hasta }: HistoryRange) {
  const [result, setResult] = useState(() => ({ desde, hasta, series: initialHistoryState() }))
  useEffect(() => startHistoricalReadingsLoad(api.getReadings, { desde, hasta }, series => {
    setResult({ desde, hasta, series })
  }), [desde, hasta])
  // No mostrar datos del rango anterior ni siquiera antes de ejecutar el efecto.
  return result.desde === desde && result.hasta === hasta ? result.series : initialHistoryState()
}
