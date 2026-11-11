import type { HistoricalReading } from '../../api/types.ts'
import { formatHistoryDate, formatHistoryValue } from '../../history/historyFormatters.ts'

export function HistoricalTooltip({ reading, unit }: { reading?: HistoricalReading; unit: string }) {
  if (!reading) return null
  return (
    <div className="history-tooltip">
      <p><strong>{formatHistoryValue(reading.valor, unit)}</strong></p>
      <time dateTime={reading.fecha_hora}>{formatHistoryDate(reading.fecha_hora)}</time>
    </div>
  )
}
