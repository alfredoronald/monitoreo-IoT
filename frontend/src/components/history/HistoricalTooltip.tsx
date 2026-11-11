import type { HistoryChartPoint } from '../../history/historyFormatters.ts'
import { formatHistoryDate, formatHistoryValue } from '../../history/historyFormatters.ts'

export function HistoricalTooltip({ reading, unit }: { reading?: HistoryChartPoint; unit: string }) {
  if (!reading) return null
  if (reading.gap || reading.valor === null) {
    const gap = reading.gap
    return (
      <div className="history-tooltip">
        <p><strong>Sin lecturas</strong></p>
        {gap && <p className="history-tooltip-note">Del {formatHistoryDate(new Date(gap.desde).toISOString())} al {formatHistoryDate(new Date(gap.hasta).toISOString())}</p>}
      </div>
    )
  }
  return (
    <div className="history-tooltip">
      <p><strong>{formatHistoryValue(reading.valor, unit)}</strong></p>
      <time dateTime={reading.fecha_hora}>{formatHistoryDate(reading.fecha_hora)}</time>
    </div>
  )
}
