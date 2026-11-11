import { useId, useRef, useState } from 'react'
import type { FormEvent } from 'react'
import type { HistoryRange } from '../../history/historicalReadings.ts'
import { quickHistoryRange, toLocalDateTime, validateCustomHistoryRange } from '../../history/historyRanges.ts'
import type { HistoryPeriod, HistoryRangeErrors } from '../../history/historyRanges.ts'

export function HistoryRangeSelector({ appliedRange, onApply }: { appliedRange: HistoryRange; onApply: (range: HistoryRange) => void }) {
  const id = useId()
  const [period, setPeriod] = useState<HistoryPeriod>('last-hour')
  const [draft, setDraft] = useState(() => ({ desde: toLocalDateTime(appliedRange.desde), hasta: toLocalDateTime(appliedRange.hasta) }))
  const [errors, setErrors] = useState<HistoryRangeErrors>({})
  const startInput = useRef<HTMLInputElement>(null)
  const endInput = useRef<HTMLInputElement>(null)

  function selectPeriod(value: HistoryPeriod) {
    setPeriod(value)
    setErrors({})
    if (value === 'custom') {
      setDraft({ desde: toLocalDateTime(appliedRange.desde), hasta: toLocalDateTime(appliedRange.hasta) })
    } else {
      onApply(quickHistoryRange(value))
    }
  }

  function applyCustomRange(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (period !== 'custom') return
    const result = validateCustomHistoryRange(draft)
    setErrors(result.errors)
    if (result.range) onApply(result.range)
    else (result.errors.desde ? startInput : endInput).current?.focus()
  }

  return (
    <form className="history-range-selector" aria-label="Periodo del histórico" noValidate onSubmit={applyCustomRange}>
      <div className="history-period-field">
        <label htmlFor={`${id}-period`}>Periodo</label>
        <select id={`${id}-period`} value={period} onChange={event => selectPeriod(event.target.value as HistoryPeriod)}>
          <option value="last-hour">Última hora</option>
          <option value="last-24-hours">Últimas 24 horas</option>
          <option value="custom">Personalizado</option>
        </select>
      </div>
      {period === 'custom' && <>
        <p id={`${id}-hint`} className="history-custom-hint">Introduce las horas de este dispositivo y pulsa Aplicar rango. Los gráficos conservan el último rango aplicado mientras editas.</p>
        <div className="history-custom-fields">
          {(['desde', 'hasta'] as const).map(field => (
            <div className="history-date-field" key={field}>
              <label htmlFor={`${id}-${field}`}>{field === 'desde' ? 'Desde' : 'Hasta'}</label>
              <input id={`${id}-${field}`} ref={field === 'desde' ? startInput : endInput} type="datetime-local" step="1"
                min="0001-01-01T00:00:00" max="9999-12-31T23:59:59" required value={draft[field]}
                aria-invalid={errors[field] ? true : undefined}
                aria-describedby={`${id}-hint${errors[field] ? ` ${id}-${field}-error` : ''}`}
                onChange={event => {
                  const value = event.target.value
                  setDraft(previous => ({ ...previous, [field]: value }))
                  setErrors({})
                }} />
              {errors[field] && <p id={`${id}-${field}-error`} className="history-field-error" role="alert">{errors[field]}</p>}
            </div>
          ))}
          <button type="submit" className="history-apply-button">Aplicar rango</button>
        </div>
      </>}
    </form>
  )
}
