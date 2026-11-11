import { useMemo } from 'react'
import { CartesianGrid, Line, LineChart, ReferenceArea, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'
import type { environmentalVariables } from '../../environmentalVariables.ts'
import type { HistoryRange, HistorySeries } from '../../history/historicalReadings.ts'
import { formatHistoryTick, formatHistoryValue, historyChartPoints, historyGaps } from '../../history/historyFormatters.ts'
import { HistoricalTooltip } from './HistoricalTooltip.tsx'

type Variable = typeof environmentalVariables[number]

export function HistoricalChart({ variable, series, range }: { variable: Variable; series: HistorySeries; range: HistoryRange }) {
  const points = useMemo(() => historyChartPoints(series.readings, range), [series.readings, range])
  const gaps = useMemo(() => historyGaps(points), [points])
  return (
    <article className={`history-card ${variable.id}`} aria-labelledby={`${variable.id}-history`} aria-busy={series.loading}>
      <div className="history-card-heading">
        <h3 id={`${variable.id}-history`}><span className="legend-dot" aria-hidden="true" />{variable.name}</h3>
        <span>{variable.unit}</span>
      </div>
      {series.loading ? <p className="history-message" role="status">Cargando histórico…</p>
        : series.error ? <p className="history-message history-error" role="alert">{series.error}</p>
        : points.length === 0 ? <p className="history-message" role="status">No hay lecturas en este rango.</p>
        : <>
          <div className="history-chart" role="group" aria-label={`Histórico de ${variable.name}, valores en ${variable.unit}`}>
            <ResponsiveContainer width="100%" height="100%" minWidth={0}>
              <LineChart data={points} margin={{ top: 12, right: 14, bottom: 8, left: 0 }} accessibilityLayer>
                {gaps.map(gap => <ReferenceArea key={`${gap.x1}-${gap.x2}`} x1={gap.x1} x2={gap.x2}
                  fill="var(--color-empty-soft)" fillOpacity={0.8} stroke="none" />)}
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
                  dot={points.length === 1 ? { r: 4 } : false} activeDot={{ r: 4 }} connectNulls={false} isAnimationActive={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
          {gaps.length > 0 && <p className="history-gap-note">Banda sombreada: el circuito no envió lecturas; la línea se interrumpe.</p>}
        </>}
    </article>
  )
}
