import type { HistoricalReading } from '../api/types.ts'

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
