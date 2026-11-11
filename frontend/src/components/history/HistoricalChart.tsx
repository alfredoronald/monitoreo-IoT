import { useMemo } from 'react'
import { CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'
import type { environmentalVariables } from '../../environmentalVariables.ts'
import type { HistoryRange, HistorySeries } from '../../history/historicalReadings.ts'
import { formatHistoryTick, formatHistoryValue, historyPoints } from '../../history/historyFormatters.ts'
import { HistoricalTooltip } from './HistoricalTooltip.tsx'

type Variable = typeof environmentalVariables[number]

export function HistoricalChart({ variable, series, range }: { variable: Variable; series: HistorySeries; range: HistoryRange }) {
  const points = useMemo(() => historyPoints(series.readings), [series.readings])
  return (
    <article className={`history-card ${variable.id}`} aria-labelledby={`${variable.id}-history`} aria-busy={series.loading}>
      <div className="history-card-heading">
        <h3 id={`${variable.id}-history`}><span className="legend-dot" aria-hidden="true" />{variable.name}</h3>
        <span>{variable.unit}</span>
      </div>
      {series.loading ? <p className="history-message" role="status">Cargando histórico…</p>
        : series.error ? <p className="history-message history-error" role="alert">{series.error}</p>
        : points.length === 0 ? <p className="history-message" role="status">No hay lecturas en este rango.</p>
        : <div className="history-chart" role="group" aria-label={`Histórico de ${variable.name}, valores en ${variable.unit}`}>
          <ResponsiveContainer width="100%" height="100%" minWidth={0}>
            <LineChart data={points} margin={{ top: 12, right: 14, bottom: 8, left: 0 }} accessibilityLayer>
              <CartesianGrid stroke="var(--color-grid)" strokeDasharray="3 3" />
              <XAxis dataKey="timestamp" type="number" scale="time" domain={[Date.parse(range.desde), Date.parse(range.hasta)]}
                tickFormatter={formatHistoryTick} minTickGap={20} tickCount={3} height={58}
                tick={{ fontSize: 11, fill: 'var(--color-text-muted)' }} />
              <YAxis width={78} domain={['auto', 'auto']} tickCount={5}
                tickFormatter={(value: number) => formatHistoryValue(value, variable.unit)}
                tick={{ fontSize: 11, fill: 'var(--color-text-muted)' }} />
              <Tooltip content={({ active, activeIndex }) => <HistoricalTooltip unit={variable.unit}
                reading={active && activeIndex != null ? points[Number(activeIndex)] : undefined} />} />
              <Line type="linear" dataKey="valor" name={variable.name} stroke="var(--variable-color)" strokeWidth={2}
                dot={points.length === 1 ? { r: 4 } : false} activeDot={{ r: 4 }} isAnimationActive={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>}
    </article>
  )
}
