import type { PendingAlertsState } from '../api/pendingAlerts.ts'

interface Props extends PendingAlertsState {
  markAttended: (id: string) => void
}

export function AlertBanner({ alerts, loading, error, attendingIds, actionErrors, markAttended }: Props) {
  if (!alerts.length) {
    if (error) return <p className="pending-alerts-error" role="status">{error}</p>
    if (loading) return <p className="pending-alerts-loading" role="status">Consultando alertas pendientes…</p>
    return null
  }

  return (
    <section className="pending-alerts" aria-labelledby="pending-alerts-title">
      <div className="pending-alerts-heading">
        <span className="pending-alerts-symbol" aria-hidden="true">!</span>
        <div>
          <h2 id="pending-alerts-title">Alertas pendientes <span>· {alerts.length}</span></h2>
          <p>Revisa los avisos registrados de calidad del aire. Atenderlos no cambia el nivel actual.</p>
        </div>
      </div>
      {error && <p className="pending-alerts-error" role="status">{error}</p>}
      <ul className="pending-alerts-list">
        {alerts.map(alert => {
          const attending = attendingIds.includes(alert.id)
          return (
            <li key={alert.id}>
              <div className="pending-alerts-message">
                <p>{alert.mensaje}</p>
                <time dateTime={alert.fecha_hora}>{new Date(alert.fecha_hora).toLocaleString('es-BO')}</time>
                {actionErrors[alert.id] && <p className="pending-alerts-error" role="alert">{actionErrors[alert.id]}</p>}
              </div>
              <button type="button" disabled={attending} onClick={() => markAttended(alert.id)} aria-label={`Marcar como atendida: ${alert.mensaje}`}>
                {attending ? 'Atendiendo…' : 'Marcar como atendida'}
              </button>
            </li>
          )
        })}
      </ul>
    </section>
  )
}
