import { useState } from 'react'
import { lastHourRange } from '../../history/historyRanges.ts'
import { HistoricalCharts } from './HistoricalCharts.tsx'
import { HistoryRangeSelector } from './HistoryRangeSelector.tsx'

export function HistoryPanel() {
  const [range, setRange] = useState(() => lastHourRange())
  return (
    <>
      <HistoryRangeSelector appliedRange={range} onApply={setRange} />
      <HistoricalCharts range={range} />
    </>
  )
}
