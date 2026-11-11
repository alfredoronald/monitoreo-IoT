import type { HistoricalReading } from '../api/types.ts'
import type { HistoryRange } from './historicalReadings.ts'

const axisDate = new Intl.DateTimeFormat('es-BO', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', hourCycle: 'h23' })
const exactDate = new Intl.DateTimeFormat('es-BO', {
  year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit',
  fractionalSecondDigits: 3, timeZoneName: 'shortOffset', hourCycle: 'h23',
})
const valueFormat = new Intl.NumberFormat('es-BO', { maximumFractionDigits: 20 })

export const formatHistoryTick = (timestamp: number) => axisDate.format(timestamp)
export const formatHistoryDate = (date: string) => exactDate.format(new Date(date))
export const formatHistoryValue = (value: number, unit: string) => `${valueFormat.format(value)} ${unit}`

// timestamp es exclusivamente de presentación; se conservan los campos originales.
export function historyPoints(readings: HistoricalReading[]) {
  return readings.map(reading => ({ ...reading, timestamp: Date.parse(reading.fecha_hora) }))
}

// Un hueco es una separación mayor que este mínimo y que 3 veces la cadencia
// habitual. Con el envío cada 15 s el umbral queda en 45 s; en series uniformes
// más espaciadas no se marcan huecos falsos.
export const HISTORY_GAP_MIN_MS = 45_000
const HISTORY_GAP_CADENCE_FACTOR = 3

export type HistoryChartPoint = HistoricalReading & { timestamp: number; held?: true }

function medianInterval(readings: HistoricalReading[]) {
  const intervals = readings.slice(1)
    .map((reading, index) => Date.parse(reading.fecha_hora) - Date.parse(readings[index].fecha_hora))
    .sort((a, b) => a - b)
  if (intervals.length === 0) return null
  const middle = Math.floor(intervals.length / 2)
  return intervals.length % 2 === 1 ? intervals[middle] : (intervals[middle - 1] + intervals[middle]) / 2
}

export function historyGapThreshold(readings: HistoricalReading[]) {
  const cadence = medianInterval(readings)
  return cadence === null ? null : Math.max(HISTORY_GAP_MIN_MS, HISTORY_GAP_CADENCE_FACTOR * cadence)
}

// Puntos de presentación: en cada hueco se repite el último valor para que el
// trazo quede horizontal y marcado, sin fabricar lecturas nuevas.
export function historyChartPoints(readings: HistoricalReading[], range: HistoryRange): HistoryChartPoint[] {
  const points = historyPoints(readings)
  const threshold = historyGapThreshold(readings)
  if (threshold === null) return points
  const chartPoints: HistoryChartPoint[] = []
  points.forEach((point, index) => {
    const previous = points[index - 1]
    if (previous && point.timestamp - previous.timestamp > threshold)
      chartPoints.push({ ...previous, fecha_hora: point.fecha_hora, timestamp: point.timestamp, held: true })
    chartPoints.push(point)
  })
  const last = points.at(-1)!
  const end = Date.parse(range.hasta)
  if (end - last.timestamp > threshold)
    chartPoints.push({ ...last, fecha_hora: range.hasta, timestamp: end, held: true })
  return chartPoints
}

export function historyGaps(points: HistoryChartPoint[]) {
  return points.flatMap((point, index) => point.held && index > 0
    ? [{ x1: points[index - 1].timestamp, x2: point.timestamp }]
    : [])
}
