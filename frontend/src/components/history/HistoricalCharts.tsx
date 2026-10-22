import { useState } from 'react'
import { environmentalVariables } from '../../environmentalVariables.ts'
import { useHistoricalReadings } from '../../hooks/useHistoricalReadings.ts'
import { lastHourRange } from '../../history/historicalReadings.ts'
import type { HistoryRange } from '../../history/historicalReadings.ts'
import { formatHistoryDate } from '../../history/historyFormatters.ts'
import { HistoricalChart } from './HistoricalChart.tsx'

export function HistoricalCharts({ range }: { range?: HistoryRange }) {
  const [initialRange] = useState(() => lastHourRange())
  const selectedRange = range ?? initialRange
  const series = useHistoricalReadings(selectedRange)
  return (
    <>
      <p className="history-range-note">
        {range ? 'Rango seleccionado' : 'Última hora al abrir el dashboard'}: {' '}
        <time dateTime={selectedRange.desde}>{formatHistoryDate(selectedRange.desde)}</time> — {' '}
        <time dateTime={selectedRange.hasta}>{formatHistoryDate(selectedRange.hasta)}</time>.
        {' '}Hora en la zona horaria de este dispositivo.
      </p>
      <div className="history-grid">
        {environmentalVariables.map(variable => <HistoricalChart key={variable.variable} variable={variable}
          series={series[variable.variable]} range={selectedRange} />)}
      </div>
    </>
  )
}
