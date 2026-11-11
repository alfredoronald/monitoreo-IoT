import type { AirState } from '../api/types.ts'
import { isReadingRecent } from '../readings/readingFreshness.ts'

type Props = {
  airState: AirState | null
  loading: boolean
  error: string | null
  now: number
}

export function AirStatusIndicator({ airState, loading, error, now }: Props) {
  const recent = airState !== null && isReadingRecent(airState.fecha_hora, now)
  const known = !recent || error !== null
  const level = airState?.nivel ?? 'empty'
  const valueFormatter = new Intl.NumberFormat('es-BO', { maximumFractionDigits: 1 })
  const timeFormatter = new Intl.DateTimeFormat('es-BO', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' })

  return (
    <section className={`air-status-panel air-status-${level}`} aria-labelledby="air-status-title" aria-live="polite" aria-busy={loading}>
      <div className="air-status-heading">
        <div>
          <p className="eyebrow">INDICADOR DE CALIDAD</p>
          <h2 id="air-status-title">Estado del aire</h2>
        </div>
        <span className="air-status-dot" aria-hidden="true" />
      </div>

      {loading && !airState ? (
        <p className="air-status-message">Cargando estado del aire…</p>
      ) : !airState ? (
        <p className="air-status-message">{error ? 'Clasificación no disponible por un error de consulta.' : 'Sin datos para clasificar.'}</p>
      ) : (
        <div className="air-status-content">
          <div className="air-status-level-row">
            <p className="air-status-level-label">{known ? 'Última clasificación conocida' : 'Nivel actual'}</p>
            <p className="air-status-level">{airState.nivel}</p>
          </div>
          <p className="air-status-tag">{airState.etiqueta}</p>
          <p className="air-status-recommendation"><strong>Recomendación:</strong> {airState.recomendacion ?? 'No hay una recomendación configurada.'}</p>
          <p className="air-status-reading">
            {known ? 'Última lectura conocida' : 'CO₂ estimado'}: {valueFormatter.format(airState.valor)} ppm · {' '}
            <time dateTime={airState.fecha_hora}>{timeFormatter.format(new Date(airState.fecha_hora))}</time>
          </p>
          {!recent && <p className="air-status-stale" role="status">Sin datos recientes (más de 60 s).</p>}
        </div>
      )}
      {error && <p className="air-status-error" role="alert">{error}{airState && ' Se conserva el último estado disponible.'}</p>}
    </section>
  )
}
