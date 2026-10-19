// Prueba opcional con navegador real y HTTP controlado. No necesita dependencias nuevas.
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
assert.ok(executable, 'Configura CHROME_PATH con la ruta de Chrome, Edge o Chromium.')
const artifactDir = process.argv.includes('--evidence') ? '../docs/evidencias/t5-6' : 'node_modules/.tmp/t5-6'
await mkdir(artifactDir, { recursive: true })
process.env.VITE_API_URL = 'http://controlled-api.test/api'
const server = await createServer({ server: { host: '127.0.0.1', port: 0 } })
await server.listen()
const address = server.httpServer.address()
const origin = `http://127.0.0.1:${address.port}`
const profile = await mkdtemp(join(tmpdir(), 'iot-range-browser-'))
const browser = spawn(executable, ['--headless=new', '--no-first-run', '--no-default-browser-check',
  '--disable-gpu', '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank'], { windowsHide: true, stdio: 'ignore' })
let socket
try {
  let port
  for (let i = 0; i < 100; i++) {
    try { port = Number((await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); break }
    catch { await new Promise(resolve => setTimeout(resolve, 100)) }
  }
  assert.ok(port, 'El navegador debe publicar su puerto de depuración.')
  const tab = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT' })).json()
  socket = new WebSocket(tab.webSocketDebuggerUrl)
  await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject })
  let id = 0
  let mode = 'data'
  let cancelled = 0
  const pending = new Map()
  const errors = []
  const calls = []
  const held = []
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    pending.set(++id, { resolve, reject })
    socket.send(JSON.stringify({ id, method, params }))
  })
  socket.onmessage = ({ data }) => {
    const message = JSON.parse(data)
    if (message.id) {
      const request = pending.get(message.id)
      pending.delete(message.id)
      if (message.error) request.reject(new Error(JSON.stringify(message.error)))
      else request.resolve(message.result)
    } else if (message.method === 'Runtime.exceptionThrown') errors.push(message.params.exceptionDetails)
    else if (message.method === 'Fetch.requestPaused') {
      const { requestId, request } = message.params
      const query = new URL(request.url).searchParams
      const variable = query.get('variable')
      const range = { desde: query.get('desde'), hasta: query.get('hasta') }
      if (variable) calls.push({ variable, ...range })
      let rows = []
      const responseCode = mode === 'partial' && variable === 'humedad' ? 500 : 200
      if (variable && mode !== 'empty' && responseCode === 200) {
        const start = Date.parse(range.desde)
        const duration = Date.parse(range.hasta) - start
        const count = duration === 0 ? 1 : duration === 86_400_000 ? 5760 : 20
        const base = { temperatura: 24, humedad: 55, co2: 850 }[variable]
        rows = Array.from({ length: count }, (_, index) => ({ id: String(index + 1), tipo_variable: variable,
          valor: Number((base + Math.sin(index / Math.max(1, count - 1) * Math.PI * 2) * (variable === 'co2' ? 100 : 2)).toFixed(2)),
          fecha_hora: new Date(start + (count === 1 ? 0 : index * duration / (count - 1))).toISOString() }))
      }
      const fulfill = () => send('Fetch.fulfillRequest', { requestId, responseCode, responseHeaders: [
        { name: 'Content-Type', value: 'application/json' }, { name: 'Access-Control-Allow-Origin', value: '*' },
      ], body: Buffer.from(JSON.stringify(responseCode === 500 ? { mensaje: 'Error controlado' } : rows)).toString('base64') })
        .catch(error => { if (error.message.includes('Invalid InterceptionId')) cancelled++; else errors.push(error.message) })
      if (mode === 'hold' && variable) held.push(fulfill)
      else void fulfill()
    }
  }
  const evaluate = async expression => {
    const result = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true })
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails))
    return result.result.value
  }
  const waitFor = async expression => {
    for (let i = 0; i < 150; i++) {
      if (await evaluate(expression)) return
      await new Promise(resolve => setTimeout(resolve, 100))
    }
    throw new Error(`No se cumplió: ${expression}`)
  }
  const key = async (key, code, number) => {
    await send('Input.dispatchKeyEvent', { type: 'keyDown', key, code, windowsVirtualKeyCode: number,
      ...(key === 'Enter' ? { text: '\r', unmodifiedText: '\r' } : {}) })
    await send('Input.dispatchKeyEvent', { type: 'keyUp', key, code, windowsVirtualKeyCode: number })
  }
  const select = async value => evaluate(`(() => { const input = document.querySelector('.history-range-selector select');
    input.value = ${JSON.stringify(value)}; input.dispatchEvent(new Event('change', {bubbles: true})); })()`)
  const setDate = async (index, value) => evaluate(`(() => { const input = document.querySelectorAll('input[type="datetime-local"]')[${index}];
    Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set.call(input, ${JSON.stringify(value)});
    input.dispatchEvent(new Event('input', {bubbles:true})); input.dispatchEvent(new Event('change', {bubbles:true})); })()`)
  const submit = async () => {
    await evaluate('document.querySelector(".history-apply-button").focus()')
    await key('Enter', 'Enter', 13)
  }
  const ready = () => waitFor('document.querySelectorAll(".history-chart .recharts-line-curve").length === 3')
  const checkCalls = async (before, duration) => {
    for (let i = 0; i < 150 && calls.length < before + 3; i++) await new Promise(resolve => setTimeout(resolve, 100))
    const batch = calls.slice(before)
    if (batch.length !== 3) console.log(await evaluate('({form:document.querySelector(".history-range-selector").innerText, values:[...document.querySelectorAll("input")].map(input=>input.value), applied:document.querySelector(".history-range-note").textContent})'))
    assert.equal(batch.length, 3)
    assert.deepEqual(batch.map(call => call.variable).sort(), ['co2', 'humedad', 'temperatura'])
    assert.ok(batch.every(call => call.desde === batch[0].desde && call.hasta === batch[0].hasta))
    assert.equal(Date.parse(batch[0].hasta) - Date.parse(batch[0].desde), duration)
  }
  const screenshots = []
  const screenshot = async (name, width) => {
    await evaluate('window.scrollTo(0, 0)')
    const layout = await send('Page.getLayoutMetrics')
    assert.equal(await evaluate('document.documentElement.scrollWidth > innerWidth'), false, `Sin overflow a ${width}px`)
    const image = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true,
      clip: { x: 0, y: 0, width, height: layout.cssContentSize.height, scale: 1 } })
    const file = `${name}-${width}.png`
    await writeFile(join(artifactDir, file), Buffer.from(image.data, 'base64'))
    screenshots.push(file)
  }
  await send('Runtime.enable')
  await send('Page.enable')
  await send('Fetch.enable', { patterns: [{ urlPattern: '*controlled-api.test/api/lecturas*', requestStage: 'Request' }] })
  for (const width of [1366, 360]) {
    mode = 'data'
    await send('Emulation.setDeviceMetricsOverride', { width, height: 900, deviceScaleFactor: 1, mobile: false })
    await send('Page.navigate', { url: origin })
    await ready()
    await screenshot('ultima-hora', width)
    let before = calls.length
    // Cambiar el select con teclado aplica el periodo rápido inmediatamente.
    await evaluate('document.querySelector(".history-range-selector select").focus()')
    await key('ArrowDown', 'ArrowDown', 40)
    await waitFor('document.querySelector("select").value === "last-24-hours"')
    await checkCalls(before, 86_400_000)
    await ready()
    await screenshot('ultimas-24-horas', width)
    before = calls.length
    await key('ArrowDown', 'ArrowDown', 40)
    await waitFor('document.querySelectorAll("input[type=datetime-local]").length === 2')
    await key('Tab', 'Tab', 9)
    assert.equal(await evaluate('document.activeElement.type'), 'datetime-local')
    await setDate(0, '2026-10-15T08:00:00')
    await setDate(1, '2026-10-15T10:00:00')
    assert.equal(calls.length, before, 'Editar el personalizado no consulta la API')
    const visited = new Set()
    for (let i = 0; i < 24; i++) {
      visited.add(await evaluate('document.activeElement.id || document.activeElement.className'))
      if (await evaluate('document.activeElement.classList.contains("history-apply-button")')) break
      await key('Tab', 'Tab', 9)
    }
    assert.ok(await evaluate('document.activeElement.classList.contains("history-apply-button")'), 'El botón es alcanzable con Tab')
    assert.ok(visited.size >= 3, 'Se recorren inicio, fin y botón')
    await key('Enter', 'Enter', 13)
    await checkCalls(before, 7_200_000)
    await ready()
    await screenshot('personalizado', width)
    before = calls.length
    await setDate(0, '')
    await submit()
    await waitFor('document.querySelector("input[aria-invalid=true]") !== null')
    assert.equal(await evaluate('document.activeElement.getAttribute("aria-invalid")'), 'true')
    await setDate(0, '2026-10-16T08:00:00')
    await submit()
    await waitFor('document.querySelector(".history-field-error").textContent.includes("igual o posterior")')
    assert.equal(calls.length, before, 'Los rangos inválidos no generan peticiones')
    await setDate(0, '2026-10-15T10:00:00')
    await submit()
    await waitFor('document.querySelectorAll(".recharts-line-dot").length === 3')
    await checkCalls(before, 0)
  }
  // Un cambio durante la carga aborta el rango anterior y conserva los datos nuevos.
  mode = 'hold'
  const cancelledBeforeReplacement = cancelled
  await select('last-hour')
  await waitFor('document.querySelectorAll(".history-card[aria-busy=true]").length === 3')
  for (let i = 0; i < 100 && held.length < 3; i++) await new Promise(resolve => setTimeout(resolve, 50))
  assert.equal(held.length, 3)
  const beforeReplacement = calls.length
  mode = 'data'
  await select('last-24-hours')
  await checkCalls(beforeReplacement, 86_400_000)
  await ready()
  const applied = await evaluate('document.querySelector(".history-range-note").textContent')
  const curves = await evaluate('[...document.querySelectorAll(".recharts-line-curve")].map(curve => curve.getAttribute("d"))')
  await Promise.all(held.map(fulfill => fulfill()))
  await new Promise(resolve => setTimeout(resolve, 100))
  assert.ok(cancelled - cancelledBeforeReplacement >= 3, 'El navegador canceló las tres peticiones obsoletas')
  assert.equal(await evaluate('document.querySelector(".history-range-note").textContent'), applied)
  assert.deepEqual(await evaluate('[...document.querySelectorAll(".recharts-line-curve")].map(curve => curve.getAttribute("d"))'), curves)
  mode = 'empty'
  await select('last-hour')
  await waitFor('[...document.querySelectorAll(".history-message")].length === 3 && [...document.querySelectorAll(".history-message")].every(item => item.textContent.includes("No hay lecturas"))')
  mode = 'partial'
  await select('last-24-hours')
  await waitFor('document.querySelectorAll(".history-error").length === 1 && document.querySelectorAll(".history-chart").length === 2')
  assert.equal(errors.length, 0, JSON.stringify(errors))
  const report = { browser: await send('Browser.getVersion'), controlledData: true, screenshots,
    cancelledObsoleteRequests: cancelled - cancelledBeforeReplacement,
    checks: ['periodos rápidos', 'tres consultas por rango', 'edición sin peticiones', 'validación', 'extremos iguales',
      'cancelación', 'carga', 'vacío', 'error parcial', 'Tab y Enter', '360 y 1366 px', '5760 puntos por variable en 24 h'], errors }
  await writeFile(join(artifactDir, 'resultados-navegador.json'), JSON.stringify(report, null, 2) + '\n')
  console.log(JSON.stringify(report, null, 2))
} finally {
  socket?.close()
  browser.kill()
  await server.close()
  // Solo eliminar el perfil temporal creado por esta ejecución, dentro de tmpdir.
  const resolvedProfile = resolve(profile)
  if (dirname(resolvedProfile) === resolve(tmpdir()) && basename(resolvedProfile).startsWith('iot-range-browser-')) {
    await rm(resolvedProfile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(() => {})
  }
}
