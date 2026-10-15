import './App.css'

const variables = [
  { id: 'temperature', name: 'Temperatura', unit: '°C', description: 'Temperatura del aula', symbol: 'T' },
  { id: 'humidity', name: 'Humedad', unit: '% HR', description: 'Humedad relativa', symbol: 'H' },
  { id: 'co2', name: 'CO₂ estimado', unit: 'ppm', description: 'Estimación del MQ-135', symbol: 'CO₂' },
]

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
            <span className="connection-status"><span aria-hidden="true" />Sin lecturas disponibles</span>
          </div>
          <div className="metrics-grid" aria-label="Valores actuales">
            {variables.map(variable => (
              <article className={`metric-card ${variable.id}`} key={variable.id} aria-labelledby={`${variable.id}-title`}>
                <div className="metric-heading">
                  <h2 id={`${variable.id}-title`}>{variable.name}</h2>
                  <span className="variable-symbol" aria-hidden="true"><VariableIcon variable={variable.id} /></span>
                </div>
                <p className="metric-value"><span aria-label="Sin valor">—</span><span className="metric-unit">{variable.unit}</span></p>
                <p className="metric-description">{variable.description}</p>
                <div className="metric-footer"><span>Sin datos</span><span>Última lectura: —</span></div>
              </article>
            ))}
          </div>
        </section>

        <section id="historico" className="history-section" aria-labelledby="history-title">
          <div className="section-heading">
            <div><p className="eyebrow">EVOLUCIÓN DEL AMBIENTE</p><h2 id="history-title">Histórico de lecturas</h2></div>
            <p className="history-description">Una mirada a cada variable a lo largo del tiempo.</p>
          </div>
          <div className="history-grid">
            {variables.map(variable => (
              <article className={`history-card ${variable.id}`} key={variable.id} aria-labelledby={`${variable.id}-history`}>
                <div className="history-card-heading"><h3 id={`${variable.id}-history`}><span className="legend-dot" aria-hidden="true" />{variable.name}</h3><span>{variable.unit}</span></div>
                <div className="chart-placeholder">
                  <div className="empty-chart-message"><span className="empty-chart-icon" aria-hidden="true">∿</span><p>Aún no hay lecturas</p><span>El histórico se mostrará aquí.</span></div>
                </div>
              </article>
            ))}
          </div>
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
