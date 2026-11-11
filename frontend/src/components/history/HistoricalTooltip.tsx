import type { HistoricalReading } from '../../api/types.ts'
import { formatHistoryDate, formatHistoryValue } from '../../history/historyFormatters.ts'

export function HistoricalTooltip({ reading, unit }: { reading?: HistoricalReading & { held?: true }; unit: string }) {
  if (!reading) return null
  return (
    <div className="history-tooltip">
      {reading.held
        ? <><p><strong>Sin lectura</strong></p><p className="history-tooltip-note">Se mantiene el valor anterior.</p></>
        : <p><strong>{formatHistoryValue(reading.valor, unit)}</strong></p>}
      <time dateTime={reading.fecha_hora}>{formatHistoryDate(reading.fecha_hora)}</time>
    </div>
  )
}
