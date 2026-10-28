import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { createServer } from 'vite'

// Esta prueba utiliza la API local real y la configuración privada de frontend/.env.
const artifacts = '../docs/evidencias/clave-api-132'
await mkdir(artifacts, { recursive: true })
const server = await createServer({ server: { host: 'localhost', port: 5173, strictPort: true } })
await server.listen()
const profile = await mkdtemp(join(tmpdir(), 'iot-api-key-'))
const browser = spawn(process.env.CHROME_PATH ?? 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  ['--headless=new', '--no-first-run', '--disable-gpu', '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank'],
  { windowsHide: true, stdio: 'ignore' })
let socket
const pending = new Map()
try {
  let port
  for (let i = 0; i < 100; i++) {
    try { port = Number((await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); break }
    catch { await new Promise(resolve => setTimeout(resolve, 100)) }
  }
  assert.ok(port, 'El navegador debe iniciar.')
  const tab = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT' })).json()
  socket = new WebSocket(tab.webSocketDebuggerUrl)
  await new Promise(resolve => { socket.onopen = resolve })
  let id = 0
  let mode = 'correcta'
  const requests = []
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const requestId = ++id
    pending.set(requestId, { resolve, reject })
    socket.send(JSON.stringify({ id: requestId, method, params }))
  })
  socket.onmessage = ({ data }) => {
    const message = JSON.parse(data)
    if (message.id) {
      const request = pending.get(message.id)
      pending.delete(message.id)
      if (message.error) request.reject(new Error(message.error.message))
      else request.resolve(message.result)
    } else if (message.method === 'Fetch.requestPaused') {
      const { requestId, request } = message.params
      if (request.method === 'OPTIONS') { void send('Fetch.continueRequest', { requestId }).catch(() => {}); return }
      const headers = Object.entries(request.headers).filter(([name]) => name.toLowerCase() !== 'x-api-key')
      const key = Object.entries(request.headers).find(([name]) => name.toLowerCase() === 'x-api-key')?.[1]
      requests.push({ mode, path: new URL(request.url).pathname, hasKey: Boolean(key) })
      if (mode === 'correcta') headers.push(['X-API-Key', key ?? ''])
      if (mode === 'incorrecta') headers.push(['X-API-Key', 'test-invalid-key'])
      void send('Fetch.continueRequest', { requestId, headers: headers.map(([name, value]) => ({ name, value })) }).catch(() => {})
    }
  }
  const evaluate = async expression => (await send('Runtime.evaluate', { expression, returnByValue: true })).result.value
  const waitFor = async expression => {
    for (let i = 0; i < 200; i++) {
      if (await evaluate(expression)) return
      await new Promise(resolve => setTimeout(resolve, 100))
    }
    throw new Error(`No se cumplió: ${expression}`)
  }
  await send('Page.enable')
  await send('Fetch.enable', { patterns: [{ urlPattern: 'http://localhost:3000/api/*', requestStage: 'Request' }] })
  for (mode of ['correcta', 'incorrecta', 'ausente']) {
    await send('Page.navigate', { url: 'http://localhost:5173' })
    await waitFor(mode === 'correcta'
      ? 'document.querySelector(".metric-footer time") !== null && !document.querySelector(".readings-error")'
      : 'document.querySelector(".readings-error")?.textContent.includes("No autorizado: revisa la clave de la API")')
    const capture = await send('Page.captureScreenshot', { format: 'png' })
    await writeFile(join(artifacts, `clave-${mode}.png`), Buffer.from(capture.data, 'base64'))
  }
  assert.ok(requests.length > 0 && requests.every(request => request.hasKey), 'El cliente envía la clave en todas sus solicitudes.')
  await writeFile(join(artifacts, 'resultados.json'), JSON.stringify({ apiReal: true, requests }, null, 2) + '\n')
  console.log(JSON.stringify({ apiReal: true, cases: ['correcta', 'incorrecta', 'ausente'], requests: requests.length }))
} finally {
  socket?.close()
  browser.kill()
  await server.close()
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(() => {})
}
