import { lastHourRange } from './historicalReadings.ts'
import type { HistoryRange } from './historicalReadings.ts'

export type HistoryPeriod = 'last-hour' | 'last-24-hours' | 'custom'
export type CustomHistoryDates = { desde: string; hasta: string }
export type HistoryRangeErrors = Partial<Record<keyof CustomHistoryDates, string>>

export { lastHourRange }

export function quickHistoryRange(period: Exclude<HistoryPeriod, 'custom'>, now = Date.now()): HistoryRange {
  if (period === 'last-hour') return lastHourRange(now)
  return { desde: new Date(now - 86_400_000).toISOString(), hasta: new Date(now).toISOString() }
}

export function toLocalDateTime(iso: string): string {
  const date = new Date(iso)
  const pad = (value: number) => String(value).padStart(2, '0')
  return `${String(date.getFullYear()).padStart(4, '0')}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`
}

function parseLocalDateTime(value: string): Date | null {
  const match = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?$/.exec(value)
  if (!match) return null
  const [, yearText, monthText, dayText, hourText, minuteText, secondText = '0'] = match
  const [year, month, day, hour, minute, second] = [yearText, monthText, dayText, hourText, minuteText, secondText].map(Number)
  if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) return null
  // setFullYear evita convertir los años 0001–0099 en 1901–1999.
  const date = new Date(0)
  date.setFullYear(year, month - 1, day)
  date.setHours(hour, minute, second, 0)
  // Date normaliza fechas imposibles y horas inexistentes por cambio de horario.
  if (!Number.isFinite(date.getTime()) || date.getFullYear() !== year || date.getMonth() !== month - 1
    || date.getDate() !== day || date.getHours() !== hour || date.getMinutes() !== minute || date.getSeconds() !== second
    || date.getUTCFullYear() < 1 || date.getUTCFullYear() > 9999) return null
  return date
}

export function validateCustomHistoryRange(draft: CustomHistoryDates): { range: HistoryRange | null; errors: HistoryRangeErrors } {
  const errors: HistoryRangeErrors = {}
  const dates = { desde: parseLocalDateTime(draft.desde), hasta: parseLocalDateTime(draft.hasta) }
  for (const field of ['desde', 'hasta'] as const) {
    if (!draft[field]) errors[field] = 'Completa la fecha y hora.'
    else if (!dates[field]) errors[field] = 'Introduce una fecha y hora existentes en la zona de este dispositivo.'
  }
  if (!dates.desde || !dates.hasta) return { range: null, errors }
  if (dates.desde.getTime() > dates.hasta.getTime()) {
    return { range: null, errors: { hasta: 'La fecha de fin debe ser igual o posterior a la de inicio.' } }
  }
  return { range: { desde: dates.desde.toISOString(), hasta: dates.hasta.toISOString() }, errors: {} }
}
