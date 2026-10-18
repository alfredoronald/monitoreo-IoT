import type { AirState } from '../api/types.ts'

type Props = {
  airState: AirState | null
  loading: boolean
  error: string | null
}

function isRecent(airState: AirState): boolean {
  return Date.now() - Date.parse(airState.fecha_hora) <= 60_000
}

export function AirStatusIndicator({ airState, loading, error }: Props) {
  const recent = airState !== null && isRecent(airState)
  const level = airState?.nivel ?? 'empty'
  const valueFormatter = new Intl.NumberFormat('es-BO', { maximumFractionDigits: 1 })
  const timeFormatter = new Intl.DateTimeFormat('es-BO', { hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' })

  return (
    <section className={`air-status-panel air-status-${level}`} aria-labelledby="air-status-title" aria-live="polite">
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
        <p className="air-status-message">Sin datos para clasificar.</p>
      ) : (
        <div className="air-status-content">
          <div className="air-status-level-row">
            <p className="air-status-level-label">Nivel actual</p>
            <p className="air-status-level">{airState.nivel}</p>
          </div>
          <p className="air-status-tag">{airState.etiqueta}</p>
          <p className="air-status-recommendation"><strong>Recomendación:</strong> {airState.recomendacion ?? 'No hay una recomendación configurada.'}</p>
          <p className="air-status-reading">
            CO₂ estimado: {valueFormatter.format(airState.valor)} ppm · Última lectura: {timeFormatter.format(new Date(airState.fecha_hora))}
          </p>
          {!recent && <p className="air-status-stale">Sin datos recientes (más de 60 s).</p>}
        </div>
      )}
      {error && <p className="air-status-error" role="alert">{error} Se conserva el último estado disponible.</p>}
    </section>
  )
}
