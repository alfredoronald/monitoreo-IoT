// Respuestas sintéticas solo en esta prueba. Nunca utiliza un backend ni sensores reales.
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { existsSync } from 'node:fs'
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { basename, dirname, join, resolve } from 'node:path'
import { createServer } from 'vite'

const executable = [process.env.CHROME_PATH,
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/usr/bin/chromium', '/usr/bin/google-chrome',
].find(path => path && existsSync(path))
assert.ok(executable, 'Configura CHROME_PATH con Chrome, Edge o Chromium.')
const artifactDir = process.argv.includes('--evidence') ? '../docs/evidencias/t5-7' : 'node_modules/.tmp/t5-7'
await mkdir(artifactDir, { recursive: true })
process.env.VITE_API_URL = 'http://controlled-api.test/api'
const server = await createServer({ server: { host: '127.0.0.1', port: 0 } })
await server.listen()
const origin = `http://127.0.0.1:${server.httpServer.address().port}`
const profile = await mkdtemp(join(tmpdir(), 'iot-states-browser-'))
const browser = spawn(executable, ['--headless=new', '--no-first-run', '--no-default-browser-check',
  '--disable-gpu', '--disable-background-timer-throttling', '--remote-debugging-port=0',
  `--user-data-dir=${profile}`, 'about:blank'], { windowsHide: true, stdio: 'ignore' })
let socket
const pending = new Map()
try {
  let port
  for (let i = 0; i < 100; i++) {
    try { port = Number((await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); break }
    catch { await new Promise(resolve => setTimeout(resolve, 100)) }
  }
  assert.ok(port, 'El navegador debe publicar su puerto de depuración.')
  const tab = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, {
    method: 'PUT', signal: AbortSignal.timeout(5000),
  })).json()
  socket = new WebSocket(tab.webSocketDebuggerUrl)
  await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('Timeout al conectar con el navegador')), 5000)
    socket.onopen = () => { clearTimeout(timeout); resolve() }
    socket.onerror = error => { clearTimeout(timeout); reject(error) }
  })
  let id = 0
  let mode = 'loading'
  let fixtureTime = Date.now()
  let attended = false
  const calls = []
  const errors = []
  const screenshots = []
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const requestId = ++id
    const timeout = setTimeout(() => { pending.delete(requestId); reject(new Error(`Timeout CDP: ${method}`)) }, 15_000)
    pending.set(requestId, { resolve, reject, timeout })
    socket.send(JSON.stringify({ id: requestId, method, params }))
  })
  const fixture = (variable, age = 0, now = fixtureTime) => ({ tipo_variable: variable,
    valor: { temperatura: 24.5, humedad: 0, co2: 850 }[variable], fecha_hora: new Date(now - age).toISOString() })
  const alert = () => ({ id: '7', lectura_id: '9', umbral_id: '2', mensaje: 'Aviso controlado de calidad del aire',
    fecha_hora: new Date(fixtureTime).toISOString(), atendida: attended })
  socket.onmessage = ({ data }) => {
    const message = JSON.parse(data)
    if (message.id) {
      const request = pending.get(message.id)
      if (!request) return
      clearTimeout(request.timeout)
      pending.delete(message.id)
      if (message.error) request.reject(new Error(JSON.stringify(message.error)))
      else request.resolve(message.result)
    } else if (message.method === 'Runtime.exceptionThrown') errors.push(message.params.exceptionDetails)
    else if (message.method === 'Fetch.requestPaused') {
      const { requestId, request } = message.params
      const url = new URL(request.url)
      const path = url.pathname
      calls.push({ path, method: request.method, at: Date.now() })
      if (mode === 'loading' || mode === 'hold-current' && ['/api/lecturas/ultima', '/api/estado-aire'].includes(path)) return
      let body = []
      let responseCode = 200
      const now = mode === 'healthy' ? Date.now() : fixtureTime
      if (request.method === 'OPTIONS') responseCode = 204
      else if (mode === 'error' || mode === 'error-current' && ['/api/lecturas/ultima', '/api/estado-aire'].includes(path)
        || mode === 'partial' && url.searchParams.get('variable') === 'humedad') {
        responseCode = 500
        body = { mensaje: 'Error controlado de la API' }
      } else if (path === '/api/lecturas/ultima') {
        body = mode === 'empty' ? [] : ['temperatura', 'humedad', 'co2'].filter(variable => mode !== 'partial' || variable !== 'humedad').map(variable => fixture(variable,
          mode === 'stale' ? 120_000 : mode === 'mixed' ? { temperatura: 55_000, humedad: 0, co2: 120_000 }[variable] : 0, now))
      } else if (path === '/api/estado-aire') {
        body = mode === 'empty' ? null : { ...fixture('co2', mode === 'stale' ? 120_000 : mode === 'mixed' ? 55_000 : 0, now),
          nivel: 'moderado', etiqueta: 'Calidad moderada', recomendacion: 'Ventilar el aula.' }
      } else if (path === '/api/lecturas') {
        const variable = url.searchParams.get('variable')
        body = mode === 'empty' || mode === 'partial' && variable === 'co2' ? [] : [0, 1].map(index => ({
          ...fixture(variable), id: String(index + 1),
          valor: fixture(variable).valor + index * (variable === 'co2' ? 50 : 1),
          fecha_hora: new Date(index === 0 ? Date.parse(url.searchParams.get('desde')) : Date.parse(url.searchParams.get('hasta')) - 120_000).toISOString(),
        }))
      } else if (request.method === 'PATCH') {
        attended = true
        body = alert()
      } else if (path === '/api/alertas') body = mode === 'empty' || attended ? [] : [alert()]
      void send('Fetch.fulfillRequest', { requestId, responseCode, responseHeaders: [
        { name: 'Content-Type', value: 'application/json' }, { name: 'Access-Control-Allow-Origin', value: '*' },
        { name: 'Access-Control-Allow-Methods', value: 'GET, PATCH, OPTIONS' },
        { name: 'Access-Control-Allow-Headers', value: 'Content-Type' },
      ], body: responseCode === 204 ? '' : Buffer.from(JSON.stringify(body)).toString('base64') }).catch(error => {
        if (!error.message.includes('Invalid InterceptionId')) errors.push(error.message)
      })
    }
  }
  const evaluate = async expression => {
    const result = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true })
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails))
    return result.result.value
  }
  const waitFor = async expression => {
    for (let i = 0; i < 220; i++) {
      if (await evaluate(expression)) return
      await new Promise(resolve => setTimeout(resolve, 100))
    }
    throw new Error(`No se cumplió: ${expression}`)
  }
  const navigate = async nextMode => {
    mode = nextMode
    fixtureTime = Date.now()
    attended = false
    await send('Page.navigate', { url: origin })
    await waitFor('document.querySelectorAll(".metric-card").length === 3')
  }
  const screenshot = async (name, width) => {
    await evaluate('window.scrollTo(0, 0)')
    assert.equal(await evaluate('document.documentElement.scrollWidth > innerWidth'), false, `Sin overflow: ${name}, ${width}px`)
    const layout = await send('Page.getLayoutMetrics')
    const capture = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true,
      clip: { x: 0, y: 0, width, height: layout.cssContentSize.height, scale: 1 } })
    const filename = `${name}-${width}.png`
    await writeFile(join(artifactDir, filename), Buffer.from(capture.data, 'base64'))
    screenshots.push(filename)
  }
  const key = async (key, code, number) => {
    await send('Input.dispatchKeyEvent', { type: 'keyDown', key, code, windowsVirtualKeyCode: number,
      ...(key === 'Enter' ? { text: '\r', unmodifiedText: '\r' } : {}) })
    await send('Input.dispatchKeyEvent', { type: 'keyUp', key, code, windowsVirtualKeyCode: number })
  }
  const checkKeyboard = async () => {
    await key('Tab', 'Tab', 9)
    assert.equal(await evaluate('document.activeElement.className'), 'skip-link')
    assert.notEqual(await evaluate('getComputedStyle(document.activeElement).outlineStyle'), 'none')
    await key('Enter', 'Enter', 13)
    assert.equal(await evaluate('document.activeElement.id'), 'contenido')
    await key('Tab', 'Tab', 9)
    assert.equal(await evaluate('document.activeElement.textContent'), 'Marcar como atendida')
    await key('Enter', 'Enter', 13)
    await waitFor('document.querySelector(".pending-alerts") === null')
    assert.equal(await evaluate('document.querySelector(".air-status-panel") !== null && document.querySelectorAll(".history-card").length === 3'), true)
  }
  await send('Runtime.enable')
  await send('Page.enable')
  await send('Page.bringToFront')
  await send('Fetch.enable', { patterns: [{ urlPattern: '*controlled-api.test/api/*', requestStage: 'Request' }] })
  for (const width of [1366, 360]) {
    await send('Emulation.setDeviceMetricsOverride', { width, height: 900, deviceScaleFactor: 1, mobile: false })
    await navigate('loading')
    assert.equal(await evaluate('document.querySelectorAll(".metric-card[aria-busy=true]").length'), 3)
    assert.equal(await evaluate('document.querySelectorAll(".history-card[aria-busy=true]").length'), 3)
    await screenshot('cargando', width)
    await navigate('error')
    await waitFor('document.querySelectorAll(".metric-status").length === 3 && [...document.querySelectorAll(".metric-status")].every(e => e.textContent.includes("No se pudieron cargar"))')
    assert.equal(await evaluate('document.querySelectorAll(".history-error").length'), 3)
    assert.equal(await evaluate('document.querySelector(".air-status-message").textContent.includes("error")'), true)
    await screenshot('error', width)
    await navigate('empty')
    await waitFor('[...document.querySelectorAll(".metric-status")].every(e => e.textContent === "Sin datos")')
    await waitFor('document.querySelectorAll(".history-message").length === 3 && [...document.querySelectorAll(".history-message")].every(e => e.textContent.includes("No hay lecturas"))')
    assert.equal(await evaluate('document.querySelector(".air-status-message").textContent'), 'Sin datos para clasificar.')
    await screenshot('sin-datos', width)
    await navigate('stale')
    await waitFor('document.querySelectorAll(".metric-stale").length === 3 && document.querySelectorAll(".history-chart .recharts-line-curve").length === 3')
    assert.equal(await evaluate('[...document.querySelectorAll(".metric-value-label")].every(e => e.textContent === "Última lectura conocida")'), true)
    assert.equal(await evaluate('document.querySelector(".air-status-level-label").textContent'), 'Última clasificación conocida')
    assert.equal(await evaluate('document.querySelector(".pending-alerts") !== null'), true)
    assert.equal(await evaluate('document.querySelectorAll(".history-card.metric-stale").length'), 0)
    await screenshot('sin-datos-recientes', width)
    await checkKeyboard()
  }

  await navigate('partial')
  await waitFor('document.querySelectorAll(".history-chart").length === 1 && document.querySelectorAll(".history-error").length === 1 && document.querySelectorAll(".history-message").length === 2')
  assert.equal(await evaluate('document.querySelector(".history-grid").textContent.includes("Sin datos recientes")'), false)
  assert.equal(await evaluate('document.querySelector(".humidity .metric-status").textContent'), 'Sin datos')
  assert.equal(await evaluate('document.querySelector(".temperature .metric-status").textContent'), 'Lectura reciente')

  await navigate('mixed')
  await waitFor('document.querySelectorAll(".metric-stale").length === 1 && document.querySelector(".air-status-level-label").textContent === "Nivel actual"')
  assert.equal(await evaluate('document.querySelector(".humidity .metric-value > span").textContent'), '0')
  const original = await evaluate('[...document.querySelectorAll(".metric-footer time")].map(e => e.dateTime)')
  const requestsBeforeExpiry = calls.filter(call => call.path === '/api/lecturas/ultima').length
  mode = 'hold-current'
  await waitFor('document.querySelector(".temperature").classList.contains("metric-stale") && document.querySelector(".air-status-level-label").textContent === "Última clasificación conocida"')
  assert.equal(await evaluate('document.querySelector(".humidity").classList.contains("metric-stale")'), false)
  assert.equal(calls.filter(call => call.path === '/api/lecturas/ultima').length, requestsBeforeExpiry, 'El reloj vence sin otra respuesta ni petición HTTP')
  assert.deepEqual(await evaluate('[...document.querySelectorAll(".metric-footer time")].map(e => e.dateTime)'), original)

  mode = 'error-current'
  await waitFor('document.querySelector(".readings-error") !== null')
  assert.deepEqual(await evaluate('[...document.querySelectorAll(".metric-footer time")].map(e => e.dateTime)'), original)
  assert.equal(await evaluate('[...document.querySelectorAll(".metric-value-label")].every(e => e.textContent === "Última lectura conocida")'), true)
  assert.equal(await evaluate('document.querySelector(".humidity .metric-status").textContent'), 'Error de actualización')
  assert.equal(await evaluate('document.querySelector(".co2 .metric-status").textContent'), 'Sin datos recientes')
  await screenshot('error-con-ultima-lectura', 360)
  mode = 'healthy'
  await waitFor('document.querySelector(".readings-error") === null && document.querySelectorAll(".metric-stale").length === 0 && document.querySelector(".air-status-level-label").textContent === "Nivel actual"')
  assert.notDeepEqual(await evaluate('[...document.querySelectorAll(".metric-footer time")].map(e => e.dateTime)'), original)
  const polling = calls.filter(call => call.path === '/api/lecturas/ultima').slice(-3)
  const intervals = polling.slice(1).map((call, index) => call.at - polling[index].at)
  assert.ok(intervals.every(interval => interval > 14_000 && interval < 17_000), JSON.stringify(intervals))
  await screenshot('recuperacion', 360)

  assert.equal(errors.length, 0, JSON.stringify(errors))
  const report = { browser: await send('Browser.getVersion'), controlledResponsesOnly: true, widths: [360, 1366], screenshots,
    pollingIntervalsMs: intervals, checks: ['carga', 'error inicial', 'respuesta vacía', 'antigüedad por variable',
      'valor cero', 'vencimiento automático sin respuestas HTTP', 'error conservando valores y fechas',
      'recuperación con muestras recientes', 'polling de 15 s', 'históricos antiguos válidos y estados independientes',
      'indicador de aire y recomendaciones', 'banner y atención de alertas', 'Tab y Enter', 'sin overflow'], errors }
  await writeFile(join(artifactDir, 'resultados-navegador.json'), JSON.stringify(report, null, 2) + '\n')
  console.log(JSON.stringify(report, null, 2))
} finally {
  for (const request of pending.values()) clearTimeout(request.timeout)
  socket?.close()
  browser.kill()
  await server.close()
  const resolvedProfile = resolve(profile)
  if (dirname(resolvedProfile) === resolve(tmpdir()) && basename(resolvedProfile).startsWith('iot-states-browser-')) {
    await rm(resolvedProfile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(() => {})
  }
}
