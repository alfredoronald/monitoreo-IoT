// T-5.8: reutiliza el transporte CDP, Vite y Fetch de las pruebas #85/#86.
// Fixtures exclusivamente en este proceso; nunca conecta con API, BD o sensores reales.
import assert from 'node:assert/strict'
import { spawn, execFileSync } from 'node:child_process'
import { createHash } from 'node:crypto'
import { existsSync } from 'node:fs'
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { basename, dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createServer } from 'vite'

const root = fileURLToPath(new URL('..', import.meta.url))
const repo = resolve(root, '..')
const output = join(repo, 'docs/evidencias/t5-8')
const option = (name, fallback) => process.argv.find(arg => arg.startsWith(`--${name}=`))?.split('=').slice(1).join('=') ?? fallback
const phase = option('phase', 'final')
assert.match(phase, /^[a-z0-9-]+$/)
const selected = option('browser', 'all')
assert.ok(['all', 'chrome', 'edge'].includes(selected), '--browser=all|chrome|edge')
const scenarios = option('scenarios', '').split(',').filter(Boolean)
const viewports = option('viewports', '360x800,768x1024,1366x768').split(',').map(value => {
  assert.match(value, /^\d{3,4}x\d{3,4}$/)
  const [width, height] = value.split('x').map(Number)
  assert.ok(width >= 320 && width <= 2560 && height >= 600 && height <= 1600)
  return { width, height }
})
const paths = {
  chrome: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  edge: process.env.EDGE_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
}
const variables = ['temperatura', 'humedad', 'co2']
const pause = ms => new Promise(resolve => setTimeout(resolve, ms))
const git = args => execFileSync('git', args, { cwd: repo, encoding: 'utf8', windowsHide: true }).trim()
const hashes = {}
for (const file of ['src/App.tsx', 'src/App.css', 'src/index.css', 'src/components/history/HistoricalChart.tsx', 'tests/responsive-dashboard.browser.mjs']) {
  hashes[file] = createHash('sha256').update(await readFile(join(root, file))).digest('hex')
}
await mkdir(output, { recursive: true })
// Sólo se cambia el entorno del proceso, sin leer/escribir .env de usuario.
process.env.VITE_API_URL = 'http://controlled-api.test/api'
process.env.VITE_API_KEY = 'test-api-key'
const server = await createServer({ root, envDir: false, server: { host: '127.0.0.1', port: 0 } })
await server.listen()
const origin = `http://127.0.0.1:${server.httpServer.address().port}`

async function runBrowser(name) {
  const executable = paths[name]
  assert.ok(existsSync(executable), `No existe ${executable}`)
  const profile = await mkdtemp(join(tmpdir(), `iot-responsive-${name}-`))
  const browser = spawn(executable, ['--headless=new', '--no-first-run', '--no-default-browser-check',
    '--disable-gpu', '--disable-background-timer-throttling', '--remote-debugging-port=0',
    `--user-data-dir=${profile}`, 'about:blank'], { windowsHide: true, stdio: 'ignore' })
  let socket
  let launchError
  browser.on('error', error => { launchError = error })
  const pending = new Map()
  const report = { browserName: name, phase, startedAt: new Date().toISOString(), timezone: 'America/La_Paz',
    configuration: { viewports, scenarios: scenarios.length ? scenarios : 'all', breakpoints: option('breakpoints', 'yes') !== 'no' },
    commit: git(['rev-parse', 'HEAD']), gitStatus: git(['status', '--short']), sourceSha256: hashes,
    platform: process.platform, node: process.version, executable, controlledResponsesOnly: true,
    physicalPhone: false, emulation: 'CSS viewport + touch emulation at 360px; mobile:false; DPR=1',
    cases: [], screenshots: [], calls: [], exceptions: [], consoleErrors: [], browserWarnings: [], cancelledRequests: 0 }
  const reportFile = join(output, `resultados-${name}${phase === 'final' ? '' : `-${phase}`}.json`)
  let viewport
  let mode = 'data'
  let level = 'moderado'
  let attended = new Set()
  let patchFails = false
  let fixtureTime = Date.now()
  let id = 0
  const held = []
  const save = async () => {
    report.finishedAt = new Date().toISOString()
    report.totals = { executed: report.cases.length, pass: report.cases.filter(c => c.status === 'PASS').length,
      fail: report.cases.filter(c => c.status === 'FAIL').length }
    await writeFile(reportFile, JSON.stringify(report, null, 2) + '\n')
  }
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const requestId = ++id
    const timeout = setTimeout(() => { pending.delete(requestId); reject(new Error(`Timeout CDP: ${method}`)) }, 20_000)
    pending.set(requestId, { resolve, reject, timeout })
    socket.send(JSON.stringify({ id: requestId, method, params }))
  })
  const evaluate = async expression => {
    const result = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true })
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails))
    return result.result.value
  }
  const waitFor = async expression => {
    for (let i = 0; i < 180; i++) {
      if (await evaluate(expression)) return
      await pause(100)
    }
    throw new Error(`No se cumplió: ${expression}`)
  }
  const shot = async title => {
    const folder = join('capturas', name, `${viewport.width}x${viewport.height}`, phase)
    await mkdir(join(output, folder), { recursive: true })
    const layout = await send('Page.getLayoutMetrics')
    const capture = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true,
      clip: { x: 0, y: 0, width: Math.max(viewport.width, Math.ceil(layout.cssContentSize.width)),
        height: Math.ceil(layout.cssContentSize.height), scale: 1 } })
    const path = join(folder, `${title}.png`).replaceAll('\\', '/')
    await writeFile(join(output, path), Buffer.from(capture.data, 'base64'))
    report.screenshots.push(path)
    return path
  }
  const geometry = () => evaluate(`(() => {
    const rect = element => { const r = element.getBoundingClientRect(); return {x:r.x,y:r.y,width:r.width,height:r.height,right:r.right,bottom:r.bottom}; };
    const selector = '.metric-card,.history-card,.air-status-panel,.pending-alerts,.history-range-selector,.metric-value,.air-status-tag,.air-status-recommendation,input,select,button';
    const elements = [...document.querySelectorAll(selector)].map(el => ({tag:el.tagName,class:el.className,
      text:el.textContent?.slice(0,100),rect:rect(el),scrollWidth:el.scrollWidth,clientWidth:el.clientWidth}));
    const outside = elements.filter(e => e.rect.x < -1 || e.rect.right > innerWidth + 1 || e.scrollWidth > e.clientWidth + 2);
    const overlaps = [];
    for (const group of ['.metrics-grid > article','.history-grid > article','#resumen > .metrics-grid,#resumen > .air-status-panel,#resumen > .pending-alerts','.history-custom-fields > *']) {
      const items = [...document.querySelectorAll(group)];
      for (let i=0;i<items.length;i++) for(let j=i+1;j<items.length;j++) {
        const a=rect(items[i]),b=rect(items[j]);
        if(Math.min(a.right,b.right)-Math.max(a.x,b.x)>1 && Math.min(a.bottom,b.bottom)-Math.max(a.y,b.y)>1) overlaps.push({group,i,j});
      }
    }
    const charts = [...document.querySelectorAll('.history-chart')].map(el => ({rect:rect(el), svg:rect(el.querySelector('svg')),
      invalidPath:[...el.querySelectorAll('path')].some(p=>/NaN|Infinity/.test(p.getAttribute('d')||''))}));
    return {innerWidth,innerHeight,scrollWidth:document.documentElement.scrollWidth,elements,outside,overlaps,charts};
  })()`)
  const layout = async () => {
    const result = await geometry()
    assert.ok(result.scrollWidth <= result.innerWidth, `Overflow de página: ${result.scrollWidth}/${result.innerWidth}`)
    assert.deepEqual(result.outside, [], 'Contenido recortado o fuera del viewport')
    assert.deepEqual(result.overlaps, [], 'Solapamiento de componentes hermanos')
    assert.ok(result.charts.every(c => c.rect.width > 150 && c.rect.height === 260 && c.svg.width > 0 && !c.invalidPath))
    return result
  }
  const run = async (title, action) => {
    if (scenarios.length && !scenarios.includes(title)) return
    const item = { id: title, viewport: { ...viewport }, startedAt: new Date().toISOString() }
    try { item.details = await action(); item.geometry = await layout(); item.status = 'PASS' }
    catch (error) { item.status = 'FAIL'; item.error = error.stack; item.geometry = await geometry().catch(() => null) }
    item.screenshot = await shot(title)
    item.finishedAt = new Date().toISOString()
    report.cases.push(item)
    console.log(`${name} ${viewport.width}x${viewport.height} ${title}: ${item.status}${item.error ? ` ${item.error.split('\n')[0]}` : ''}`)
    await save()
  }
  const key = async (key, code, number) => {
    await send('Input.dispatchKeyEvent', { type: 'keyDown', key, code, windowsVirtualKeyCode: number,
      ...(key === 'Enter' ? { text: '\r', unmodifiedText: '\r' } : {}) })
    await send('Input.dispatchKeyEvent', { type: 'keyUp', key, code, windowsVirtualKeyCode: number })
  }
  const select = async value => {
    await evaluate(`(() => {const el=document.querySelector('select');el.value=${JSON.stringify(value)};el.dispatchEvent(new Event('change',{bubbles:true}));})()`)
    await pause(60)
  }
  const setDate = async (index, value) => {
    await evaluate(`(() => {const el=document.querySelectorAll('input[type="datetime-local"]')[${index}];
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(el,${JSON.stringify(value)});
      el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));})()`)
    await pause(60)
  }
  const submit = async () => { await evaluate('document.querySelector(".history-apply-button").focus()'); await key('Enter', 'Enter', 13) }
  const ready = () => waitFor('document.querySelectorAll(".history-chart .recharts-line-curve").length === 3')
  const navigate = async (nextMode = 'data', nextLevel = 'moderado') => {
    mode = nextMode; level = nextLevel; fixtureTime = Date.now(); attended = new Set(); patchFails = false
    await send('Page.navigate', { url: origin })
    await waitFor('document.querySelectorAll(".metric-card").length === 3')
  }
  const historyCalls = () => report.calls.filter(c => c.path === '/api/lecturas' && c.method === 'GET')
  const batch = (before, duration) => {
    const calls = historyCalls().slice(before)
    assert.equal(calls.length, 3)
    assert.deepEqual(calls.map(c => c.query.variable).sort(), [...variables].sort())
    assert.ok(calls.every(c => c.query.desde === calls[0].query.desde && c.query.hasta === calls[0].query.hasta))
    assert.equal(Date.parse(calls[0].query.hasta) - Date.parse(calls[0].query.desde), duration)
    return calls
  }
  try {
    let port
    for (let i = 0; i < 120; i++) {
      if (launchError) throw launchError
      try { port = Number((await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); break }
      catch { await pause(100) }
    }
    assert.ok(port, 'El navegador debe publicar su puerto de depuración')
    const tab = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT', signal: AbortSignal.timeout(5000) })).json()
    socket = new WebSocket(tab.webSocketDebuggerUrl)
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Timeout WebSocket')), 5000)
      socket.onopen = () => { clearTimeout(timeout); resolve() }
      socket.onerror = error => { clearTimeout(timeout); reject(error) }
    })
    const reading = (variable, age = 0) => ({ tipo_variable: variable,
      valor: mode === 'long-number' ? 123456789012345.6 : { temperatura: 24.5, humedad: 0, co2: 850 }[variable],
      fecha_hora: new Date(Date.now() - age).toISOString() })
    const alerts = () => ['7', '8'].filter(id => !attended.has(id)).map(id => ({ id, lectura_id: '9', umbral_id: '2',
      mensaje: id === '7' ? `Aviso largo: ${'revisar ventilación del aula. '.repeat(12)}${'X'.repeat(90)}` : 'Segunda alerta controlada',
      fecha_hora: new Date(fixtureTime).toISOString(), atendida: false }))
    socket.onmessage = ({ data }) => {
      const message = JSON.parse(data)
      if (message.id) {
        const request = pending.get(message.id)
        if (!request) return
        clearTimeout(request.timeout); pending.delete(message.id)
        if (message.error) request.reject(new Error(JSON.stringify(message.error)))
        else request.resolve(message.result)
      } else if (message.method === 'Runtime.exceptionThrown') report.exceptions.push(message.params.exceptionDetails)
      else if (message.method === 'Log.entryAdded') {
        const entry = message.params.entry
        if (entry.level === 'error') report.consoleErrors.push(entry)
        else if (entry.level === 'warning') report.browserWarnings.push(entry)
      } else if (message.method === 'Fetch.requestPaused') {
        const { requestId, request } = message.params
        const url = new URL(request.url)
        const path = url.pathname
        const query = Object.fromEntries(url.searchParams)
        const call = { path, query, method: request.method, mode, at: Date.now() }
        report.calls.push(call)
        let responseCode = 200
        let body = []
        if (request.method === 'OPTIONS') responseCode = 204
        else if (mode === 'loading') return
        else if (mode === 'error' || mode === 'error-current' && ['/api/lecturas/ultima', '/api/estado-aire'].includes(path)
          || mode === 'partial' && query.variable === 'humedad' || request.method === 'PATCH' && patchFails) {
          responseCode = 500; body = { mensaje: 'Error HTTP controlado T-5.8' }
        } else if (path === '/api/lecturas/ultima') {
          body = mode === 'empty' ? [] : variables.filter(v => mode !== 'partial' || v !== 'humedad').map(v => reading(v, mode === 'stale' ? 120_000 : 0))
        } else if (path === '/api/estado-aire') {
          body = mode === 'empty' ? null : { ...reading('co2', mode === 'stale' ? 120_000 : 0), nivel: level,
            etiqueta: mode === 'long-air' ? 'CALIDAD'.repeat(25) : `Calidad ${level}`,
            recomendacion: mode === 'long-air' ? 'VENTILAR'.repeat(40) : 'Ventilar el aula según la clasificación recibida.' }
        } else if (path === '/api/lecturas') {
          const start = Date.parse(query.desde), duration = Date.parse(query.hasta) - start
          const count = mode === 'empty' || mode === 'partial' && query.variable === 'co2' ? 0 : duration === 0 ? 1 : duration === 86_400_000 ? 5760 : 4
          call.points = count
          call.duration = duration
          body = Array.from({ length: count }, (_, index) => ({ id: String(index + 1), tipo_variable: query.variable,
            valor: index === 0 ? 0 : Number(({ temperatura: 24, humedad: 55, co2: 850 }[query.variable] + Math.sin(index) * 2).toFixed(2)),
            fecha_hora: new Date(start + (count <= 1 ? 0 : count === 4 ? [0, 0.01, 0.25, 1][index] * duration : index * duration / (count - 1))).toISOString() }))
        } else if (request.method === 'PATCH') {
          const id = path.split('/').at(-1)
          call.postData = request.postData
          const previous = alerts().find(alert => alert.id === id)
          attended.add(id); body = { ...previous, atendida: true }
        } else if (path === '/api/alertas') body = mode === 'empty' ? [] : alerts()
        else { responseCode = 404; body = { mensaje: 'Ruta de prueba desconocida' } }
        call.status = responseCode
        const fulfill = async () => {
          try {
            await send('Fetch.fulfillRequest', { requestId, responseCode, responseHeaders: [
              { name: 'Content-Type', value: 'application/json' }, { name: 'Access-Control-Allow-Origin', value: '*' },
              { name: 'Access-Control-Allow-Methods', value: 'GET, PATCH, OPTIONS' },
              { name: 'Access-Control-Allow-Headers', value: 'Content-Type, X-API-Key' },
            ], body: responseCode === 204 ? '' : Buffer.from(JSON.stringify(body)).toString('base64') })
          } catch (error) {
            if (error.message.includes('Invalid InterceptionId')) { report.cancelledRequests++; call.cancelled = true }
            else report.exceptions.push(error.message)
          }
        }
        if (mode === 'hold-history' && path === '/api/lecturas' && request.method !== 'OPTIONS') held.push(fulfill)
        else void fulfill()
      }
    }
    await send('Runtime.enable'); await send('Page.enable'); await send('Log.enable')
    await send('Page.bringToFront')
    await send('Emulation.setTimezoneOverride', { timezoneId: 'America/La_Paz' })
    await send('Fetch.enable', { patterns: [{ urlPattern: '*controlled-api.test/api/*', requestStage: 'Request' }] })
    report.browser = await send('Browser.getVersion')
    const resize = async size => {
      viewport = size
      await send('Emulation.setDeviceMetricsOverride', { ...size, deviceScaleFactor: 1, mobile: false })
      await send('Emulation.setTouchEmulationEnabled', { enabled: size.width === 360 })
      await pause(120)
    }
    for (const size of viewports) {
      await resize(size)
      await run('01-dashboard-ultima-hora', async () => {
        const before = historyCalls().length
        await navigate(); await ready()
        await waitFor('document.querySelectorAll(".metric-footer time").length === 3 && document.querySelectorAll(".pending-alerts-list li").length === 2')
        assert.equal(await evaluate('document.querySelector(".humidity .metric-value > span").textContent'), '0')
        assert.equal(await evaluate('document.querySelectorAll("header,main,footer,nav").length'), 4)
        // React StrictMode puede montar, cancelar y volver a montar al abrir.
        const initial = historyCalls().slice(before)
        assert.ok(initial.length === 3 || initial.length === 6)
        return { initialRequestCount: initial.length, calls: batch(historyCalls().length - 3, 3_600_000), gridColumns: await evaluate('getComputedStyle(document.querySelector(".metrics-grid")).gridTemplateColumns') }
      })
      await run('02-tooltips-tres-graficos', async () => {
        const details = []
        for (let index = 0; index < 3; index++) {
          const point = await evaluate(`(() => { const el=document.querySelectorAll('.history-chart')[${index}];el.scrollIntoView({block:'center'});
            const path=el.querySelector('.recharts-line-curve'),vertices=[...path.getAttribute('d').matchAll(/[ML]([\\d.-]+),([\\d.-]+)/g)];
            const p={x:Number(vertices[2][1]),y:Number(vertices[2][2])},m=path.getScreenCTM();
            return {x:m.a*p.x+m.c*p.y+m.e,y:m.b*p.x+m.d*p.y+m.f,vertices:vertices.map(v=>({x:Number(v[1]),y:Number(v[2])}))};})()`)
          assert.equal(point.vertices.length, 4)
          const span = point.vertices[3].x - point.vertices[0].x
          assert.ok(Math.abs((point.vertices[1].x - point.vertices[0].x) / span - 0.01) < 0.01)
          assert.ok(Math.abs((point.vertices[2].x - point.vertices[0].x) / span - 0.25) < 0.01, 'Distancias X proporcionales a intervalos irregulares')
          // El tercer punto está dentro del área de trazado. El borde exacto
          // del primer punto depende del redondeo del evento y no es un hit target estable.
          await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: point.x, y: point.y })
          await waitFor(`document.querySelectorAll('.history-chart')[${index}].querySelector('.history-tooltip time') !== null`)
          const detail = await evaluate(`(() => {const chart=document.querySelectorAll('.history-chart')[${index}],el=chart.querySelector('.history-tooltip'),r=el.getBoundingClientRect(),c=chart.closest('article').getBoundingClientRect();
            return {text:el.innerText,date:el.querySelector('time').dateTime,left:r.left,right:r.right,cardLeft:c.left,cardRight:c.right};})()`)
          const value = Number(({ temperatura: 24, humedad: 55, co2: 850 }[variables[index]] + Math.sin(2) * 2).toFixed(2))
          const expected = `${new Intl.NumberFormat('es-BO', { maximumFractionDigits: 20 }).format(value)} ${{ temperatura: '°C', humedad: '% HR', co2: 'ppm' }[variables[index]]}`
          assert.ok(detail.text.startsWith(expected), `Tooltip esperado ${expected}: ${detail.text}`)
          assert.match(detail.text, /GMT|UTC/)
          assert.ok(detail.left >= detail.cardLeft - 1 && detail.right <= detail.cardRight + 1, 'Tooltip dentro de su tarjeta')
          details.push({ ...detail, plotVertices: point.vertices })
          await shot(`02-tooltip-${variables[index]}`)
        }
        await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: 1, y: 1 })
        return details
      })
      await run('03-24-horas-5760-puntos', async () => {
        const before = historyCalls().length
        await select('last-24-hours'); await ready()
        const calls = batch(before, 86_400_000)
        assert.ok(calls.every(c => c.points === 5760))
        return { calls }
      })
      await run('04-personalizado-valido-teclado', async () => {
        await select('custom')
        const before = historyCalls().length
        await setDate(0, '2026-10-15T08:00:00'); await setDate(1, '2026-10-15T10:00:00')
        assert.equal(historyCalls().length, before, 'Editar no consulta')
        await evaluate('document.querySelector("select").focus()')
        const visited = new Set()
        for (let i = 0; i < 30; i++) {
          await key('Tab', 'Tab', 9)
          visited.add(await evaluate('document.activeElement.id'))
          if (await evaluate('document.activeElement.classList.contains("history-apply-button")')) break
        }
        assert.ok(await evaluate('document.activeElement.classList.contains("history-apply-button")'), 'Aplicar alcanzable por Tab')
        assert.ok(visited.size >= 3, 'Se recorren ambos campos y botón')
        assert.equal(await evaluate('getComputedStyle(document.activeElement).outlineStyle'), 'solid')
        await key('Enter', 'Enter', 13); await ready()
        return { calls: batch(before, 7_200_000), visited: [...visited] }
      })
      await run('05-fechas-vacias-invalidas', async () => {
        const before = historyCalls().length
        await setDate(0, ''); await submit()
        await waitFor('document.activeElement.getAttribute("aria-invalid") === "true"')
        await setDate(0, '2026-02-30T08:00:00'); await submit()
        await waitFor('document.querySelector("input[aria-invalid=true]") !== null')
        assert.equal(historyCalls().length, before)
        return { values: await evaluate('[...document.querySelectorAll("input")].map(el=>el.value)'), note: 'El input nativo sanea la fecha imposible a vacío; el validador lo rechaza.' }
      })
      await run('06-fechas-invertidas', async () => {
        const before = historyCalls().length
        await setDate(0, '2026-10-16T08:00:00'); await submit()
        await waitFor('document.querySelector(".history-field-error")?.textContent.includes("igual o posterior")')
        assert.equal(historyCalls().length, before)
      })
      await run('07-fechas-iguales-un-punto', async () => {
        const before = historyCalls().length
        await setDate(0, '2026-10-15T10:00:00'); await submit()
        await waitFor('document.querySelectorAll(".recharts-line-dot").length === 3')
        return { calls: batch(before, 0) }
      })
      await run('08-cambio-rapido-cancelacion', async () => {
        mode = 'hold-history'
        const cancelledBefore = report.cancelledRequests
        await select('last-hour')
        await waitFor('document.querySelectorAll(".history-card[aria-busy=true]").length === 3')
        for (let i = 0; held.length < 3 && i < 100; i++) await pause(50)
        assert.equal(held.length, 3)
        mode = 'data'
        const before = historyCalls().length
        await select('last-24-hours'); await ready()
        const curves = await evaluate('[...document.querySelectorAll(".recharts-line-curve")].map(el=>el.getAttribute("d"))')
        await Promise.all(held.splice(0).map(fulfill => fulfill())); await pause(150)
        assert.equal(report.cancelledRequests - cancelledBefore, 3)
        assert.deepEqual(await evaluate('[...document.querySelectorAll(".recharts-line-curve")].map(el=>el.getAttribute("d"))'), curves)
        return { cancelled: report.cancelledRequests - cancelledBefore, calls: batch(before, 86_400_000) }
      })
      for (const [title, state, expression] of [
        ['09-cargando', 'loading', 'document.querySelectorAll(".metric-card[aria-busy=true]").length === 3 && document.querySelectorAll(".history-card[aria-busy=true]").length === 3 && document.querySelector(".air-status-panel").getAttribute("aria-busy") === "true"'],
        ['10-error-inicial', 'error', 'document.querySelectorAll(".history-error").length === 3 && document.querySelector(".readings-error") !== null && document.querySelector(".air-status-error") !== null'],
        ['11-sin-datos', 'empty', 'document.querySelectorAll(".history-message").length === 3 && [...document.querySelectorAll(".metric-status")].every(el=>el.textContent === "Sin datos") && document.querySelector(".air-status-message")?.textContent === "Sin datos para clasificar." && document.querySelector(".pending-alerts") === null'],
        ['12-datos-antiguos', 'stale', 'document.querySelectorAll(".metric-stale").length === 3 && document.querySelector(".air-status-level-label")?.textContent === "Última clasificación conocida" && document.querySelectorAll(".recharts-line-curve").length === 3'],
        ['13-error-parcial', 'partial', 'document.querySelectorAll(".history-chart").length === 1 && document.querySelectorAll(".history-error").length === 1 && document.querySelectorAll(".history-message").length === 2 && document.querySelector(".humidity .metric-status")?.textContent === "Sin datos"'],
      ]) await run(title, async () => { await navigate(state); await waitFor(expression) })
      for (const classification of ['bueno', 'moderado', 'malo']) {
        await run(`14-aire-${classification}`, async () => {
          await navigate('data', classification); await ready()
          await waitFor(`document.querySelector('.air-status-level')?.textContent === '${classification}'`)
          const details = await evaluate('({level:document.querySelector(".air-status-level").textContent,color:getComputedStyle(document.querySelector(".air-status-level")).color,recommendation:document.querySelector(".air-status-recommendation").textContent})')
          assert.equal(details.color, { bueno: 'rgb(22, 101, 52)', moderado: 'rgb(133, 77, 14)', malo: 'rgb(153, 27, 27)' }[classification])
          assert.match(details.recommendation, /Ventilar/)
          return details
        })
      }
      await run('15-alertas-patch-fallido-reintento', async () => {
        await navigate(); await ready()
        await waitFor('document.querySelectorAll(".pending-alerts-list li").length === 2')
        patchFails = true
        await evaluate('document.querySelector(".pending-alerts-list button").focus()')
        assert.equal(await evaluate('getComputedStyle(document.activeElement).outlineStyle'), 'solid')
        await key('Enter', 'Enter', 13)
        await waitFor('document.querySelector(".pending-alerts-list .pending-alerts-error") !== null')
        assert.equal(await evaluate('document.querySelectorAll(".pending-alerts-list li").length'), 2)
        await shot('15-patch-fallido')
        patchFails = false
        if (viewport.width === 360) {
          const p = await evaluate('(() => { const el=document.querySelector(".pending-alerts-list button");el.scrollIntoView({block:"center"});const r=el.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()')
          await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: p.x, y: p.y }] })
          await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] })
        } else { await evaluate('document.querySelector(".pending-alerts-list button").focus()'); await key('Enter', 'Enter', 13) }
        await waitFor('document.querySelectorAll(".pending-alerts-list li").length === 1')
        await evaluate('document.querySelector(".pending-alerts-list button").focus()'); await key('Enter', 'Enter', 13)
        await waitFor('document.querySelector(".pending-alerts") === null')
        assert.equal(await evaluate('document.querySelectorAll(".recharts-line-curve").length'), 3)
        assert.equal(await evaluate('document.querySelector(".air-status-level").textContent'), 'moderado')
        const calls = report.calls.filter(c => c.method === 'PATCH').slice(-3)
        assert.deepEqual(calls.map(c => c.status), [500, 200, 200])
        assert.ok(calls.filter(c => c.status === 200).every(c => JSON.parse(c.postData).atendida === true))
        return { calls, touchRetry: viewport.width === 360 }
      })
      await run('16-navegacion-foco-controles', async () => {
        await navigate(); await ready()
        await key('Tab', 'Tab', 9)
        assert.equal(await evaluate('document.activeElement.className'), 'skip-link')
        assert.equal(await evaluate('getComputedStyle(document.activeElement).outlineStyle'), 'solid')
        await key('Enter', 'Enter', 13)
        assert.equal(await evaluate('document.activeElement.id'), 'contenido')
        await evaluate('document.querySelector("nav a:last-child").focus()'); await key('Enter', 'Enter', 13)
        assert.equal(await evaluate('location.hash'), '#historico')
        await select('custom')
        const controls = await evaluate('[...document.querySelectorAll(".history-range-selector input,.history-range-selector select,.history-range-selector button,.pending-alerts-list button")].map(el=>({tag:el.tagName,label:el.labels?.[0]?.textContent||el.textContent,height:el.getBoundingClientRect().height,width:el.getBoundingClientRect().width}))')
        assert.ok(controls.every(c => c.label && c.height >= 44 && c.width > 40), 'Controles etiquetados y área de interacción de al menos 44px de alto')
        return { controls }
      })
      await run('17-valores-numericos-largos', async () => { await navigate('long-number'); await ready(); await waitFor('document.querySelector(".metric-value").textContent.includes("123")') })
      await run('18-textos-aire-sin-espacios', async () => { await navigate('long-air'); await ready(); await waitFor('document.querySelector(".air-status-tag").textContent.length > 100') })
      await run('19-polling-error-conserva-recuperacion', async () => {
        await navigate(); await ready()
        const timestamps = await evaluate('[...document.querySelectorAll(".metric-footer time")].map(el=>el.dateTime)')
        const before = report.calls.length
        mode = 'error-current'
        await waitFor('document.querySelector(".readings-error") !== null')
        assert.deepEqual(await evaluate('[...document.querySelectorAll(".metric-footer time")].map(el=>el.dateTime)'), timestamps)
        await shot('19-error-conserva-valores')
        mode = 'data'
        await waitFor('document.querySelector(".readings-error") === null && document.querySelector(".air-status-level-label").textContent === "Nivel actual"')
        assert.notDeepEqual(await evaluate('[...document.querySelectorAll(".metric-footer time")].map(el=>el.dateTime)'), timestamps)
        return { pollingCalls: report.calls.slice(before).filter(c => c.path === '/api/lecturas/ultima') }
      })
    }
    if (option('breakpoints', 'yes') !== 'no') {
      for (const width of [599, 600, 601, 619, 620, 621, 899, 900, 901]) {
        await resize({ width, height: 900 })
        await run('20-breakpoint', async () => {
          await navigate(); await ready(); await select('custom')
          await waitFor('document.querySelectorAll("input").length === 2')
          const details = await evaluate('({columns:getComputedStyle(document.querySelector(".metrics-grid")).gridTemplateColumns.split(" ").length,alertDirection:getComputedStyle(document.querySelector(".pending-alerts-list li")).flexDirection,customColumns:getComputedStyle(document.querySelector(".history-custom-fields")).gridTemplateColumns.split(" ").length,applyWidth:document.querySelector(".history-apply-button").getBoundingClientRect().width,formWidth:document.querySelector(".history-custom-fields").getBoundingClientRect().width})')
          assert.equal(details.columns, width <= 900 ? 1 : 3)
          assert.equal(details.customColumns, width <= 900 ? 1 : 3)
          assert.equal(details.alertDirection, width <= 600 ? 'column' : 'row')
          if (width <= 620) assert.ok(Math.abs(details.applyWidth - details.formWidth) <= 1)
          return details
        })
      }
    }
    // Los errores HTTP 500 de fixtures son intencionados; otras excepciones no.
    await resize(viewports.at(-1))
    await run('21-sin-excepciones-javascript', async () => {
      assert.deepEqual(report.exceptions, [])
      assert.deepEqual(report.consoleErrors.filter(e => !/500/.test(e.text)), [], 'Sin errores de consola inesperados')
      return { expectedHttpErrors: report.consoleErrors.filter(e => /500/.test(e.text)).length }
    })
    await save()
    return report.totals
  } catch (error) {
    report.infrastructureError = error.stack
    await save()
    throw error
  } finally {
    for (const request of pending.values()) clearTimeout(request.timeout)
    socket?.close()
    if (browser.exitCode === null) {
      const exited = new Promise(resolve => browser.once('exit', resolve))
      browser.kill(); await Promise.race([exited, pause(3000)])
    }
    // Verificar ruta absoluta y prefijo antes de eliminar sólo nuestro perfil temporal.
    const resolvedProfile = resolve(profile)
    if (dirname(resolvedProfile) === resolve(tmpdir()) && basename(resolvedProfile).startsWith(`iot-responsive-${name}-`)) {
      await rm(resolvedProfile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(error => console.error(`Perfil temporal: ${error.message}`))
    }
  }
}

try {
  for (const name of selected === 'all' ? ['chrome', 'edge'] : [selected]) {
    const totals = await runBrowser(name)
    console.log(JSON.stringify({ browser: name, phase, ...totals }))
    if (totals.fail) process.exitCode = 1
  }
} finally {
  await server.close()
}
