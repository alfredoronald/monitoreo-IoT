import './App.css'
import { useLatestReadings } from './hooks/useLatestReadings'
import { environmentalVariables as variables } from './environmentalVariables'
import { HistoryPanel } from './components/history/HistoryPanel'
import { AirStatusIndicator } from './components/AirStatusIndicator'
import { useAirState } from './hooks/useAirState'
import { AlertBanner } from './components/AlertBanner'
import { usePendingAlerts } from './hooks/usePendingAlerts'
import { useReadingClock } from './hooks/useReadingClock'
import { isReadingRecent } from './readings/readingFreshness'

function VariableIcon({ variable }: { variable: string }) {
  if (variable === 'co2') return <>CO₂</>

  return (
    <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" focusable="false">
      {variable === 'temperature'
        ? <><path d="M9 14.5V5a3 3 0 0 1 6 0v9.5a5 5 0 1 1-6 0Z" /><path d="M12 8v10" /><circle cx="12" cy="18" r="1.5" /></>
        : <path d="M12 3C10 6 5 11 5 15a7 7 0 0 0 14 0c0-4-5-9-7-12Z" />}
    </svg>
  )
}

function App() {
  const { readings, loading, error } = useLatestReadings()
  const airStatus = useAirState()
  const pendingAlerts = usePendingAlerts()
  const now = useReadingClock([...readings.map(reading => reading.fecha_hora), ...(airStatus.airState ? [airStatus.airState.fecha_hora] : [])])
  const hasStaleReadings = readings.some(reading => !isReadingRecent(reading.fecha_hora, now))
  const valueFormatter = new Intl.NumberFormat('es-BO', { maximumFractionDigits: 1 })
  const timeFormatter = new Intl.DateTimeFormat('es-BO', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' })
  return (
    <>
      <a className="skip-link" href="#contenido">Saltar al contenido</a>
      <header className="site-header">
        <div className="header-inner">
          <a className="brand" href="#resumen" aria-label="Monitoreo IoT, ir al resumen">
            <span className="brand-mark" aria-hidden="true">m</span>
            <span>Monitoreo IoT<span className="brand-caption">Calidad ambiental del aula</span></span>
          </a>
          <nav aria-label="Navegación principal">
            <a href="#resumen">Resumen</a>
            <a href="#historico">Histórico</a>
          </nav>
          <span className="project-label">Proyecto IoT <span> / </span> Grupo 22</span>
        </div>
      </header>

      <main id="contenido" className="dashboard" tabIndex={-1}>
        <section id="resumen" aria-labelledby="overview-title">
          <div className="section-heading overview-heading">
            <div>
              <p className="eyebrow">CONDICIONES AMBIENTALES</p>
              <h1 id="overview-title">El ambiente, de un vistazo.</h1>
              <p className="section-description">Temperatura, humedad y calidad del aire en un solo lugar.</p>
            </div>
            <span className="connection-status" role="status"><span aria-hidden="true" />{loading ? 'Cargando lecturas…' : error ? 'Error de actualización' : hasStaleReadings ? 'Hay lecturas sin datos recientes' : readings.length ? 'Actualización cada 15 s' : 'Sin lecturas disponibles'}</span>
          </div>
          {error && <p className="readings-error" role="alert">{error}{readings.length > 0 && ' Se conservan los últimos valores recibidos con su hora original.'}</p>}
          <p className="reading-time-note">Hora de lectura en la zona horaria de este dispositivo.</p>
          <div className="metrics-grid" aria-label="Lecturas ambientales">
            {variables.map(variable => {
              const reading = readings.filter(item => item.tipo_variable === variable.variable).sort((left, right) => Date.parse(right.fecha_hora) - Date.parse(left.fecha_hora))[0]
              const stale = reading !== undefined && !isReadingRecent(reading.fecha_hora, now)
              const known = stale || error !== null
              const status = reading ? (stale ? 'Sin datos recientes' : error ? 'Error de actualización' : 'Lectura reciente') : loading ? 'Cargando…' : error ? 'No se pudieron cargar los datos' : 'Sin datos'
              return (
              <article className={`metric-card ${variable.id}${stale ? ' metric-stale' : ''}`} key={variable.id} aria-labelledby={`${variable.id}-title`} aria-busy={loading}>
                <div className="metric-heading">
                  <h2 id={`${variable.id}-title`}>{variable.name}</h2>
                  <span className="variable-symbol" aria-hidden="true"><VariableIcon variable={variable.id} /></span>
                </div>
                <p className="metric-value-label">{reading ? (known ? 'Última lectura conocida' : 'Lectura actual') : 'Valor no disponible'}</p>
                <p className="metric-value"><span aria-label={reading ? undefined : 'Sin valor'}>{reading ? valueFormatter.format(reading.valor) : '—'}</span><span className="metric-unit">{variable.unit}</span></p>
                <p className="metric-description">{variable.description}</p>
                <div className="metric-footer"><span className="metric-status" role="status">{status}</span><span>Última lectura: {reading ? <time dateTime={reading.fecha_hora}>{timeFormatter.format(new Date(reading.fecha_hora))}</time> : '—'}</span></div>
              </article>
            )})}
          </div>
          <AirStatusIndicator {...airStatus} now={now} />
          <AlertBanner {...pendingAlerts} />
        </section>

        <section id="historico" className="history-section" aria-labelledby="history-title">
          <div className="section-heading">
            <div><p className="eyebrow">EVOLUCIÓN DEL AMBIENTE</p><h2 id="history-title">Histórico de lecturas</h2></div>
            <p className="history-description">Una mirada a cada variable a lo largo del tiempo.</p>
          </div>
          <HistoryPanel />
        </section>

        <aside className="measurement-note" aria-label="Acerca de las mediciones">
          <span className="note-symbol" aria-hidden="true">i</span>
          <p><strong>Sobre el CO₂ estimado.</strong> El MQ-135 ofrece una estimación provisional de la calidad del aire. Sus valores requieren calibración.</p>
        </aside>
        <footer className="dashboard-footer"><span>Monitoreo ambiental del aula</span><span>Servicios Telemáticos · Grupo 22</span></footer>
      </main>
    </>
  )
}

export default App
