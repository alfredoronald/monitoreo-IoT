import assert from 'node:assert/strict'
import test from 'node:test'
import { mkdtemp, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createServer } from 'vite'

async function environment(t, content) {
  const envDir = await mkdtemp(join(tmpdir(), 'iot-api-env-'))
  await writeFile(join(envDir, '.env'), content)
  const previousUrl = process.env.VITE_API_URL
  const previousKey = process.env.VITE_API_KEY
  delete process.env.VITE_API_URL
  delete process.env.VITE_API_KEY
  const server = await createServer({
    root: fileURLToPath(new URL('..', import.meta.url)),
    configFile: false, envDir, mode: 'test', logLevel: 'silent',
    server: { middlewareMode: true, hmr: false },
  })
  t.after(async () => {
    await server.close()
    await rm(envDir, { recursive: true, force: true })
    if (previousUrl === undefined) delete process.env.VITE_API_URL
    else process.env.VITE_API_URL = previousUrl
    if (previousKey === undefined) delete process.env.VITE_API_KEY
    else process.env.VITE_API_KEY = previousKey
  })
  return server
}

test('Vite carga la URL y clave del archivo .env para cada solicitud', async t => {
  const server = await environment(t, 'VITE_API_URL=http://configured.example.test:4500/custom-api\nVITE_API_KEY=test-api-key\n')
  const original = globalThis.fetch
  let requestedUrl
  let requestedOptions
  globalThis.fetch = async (url, options) => { requestedUrl = url; requestedOptions = options; return Response.json([]) }
  t.after(() => { globalThis.fetch = original })
  const { api } = await server.ssrLoadModule('/src/api/index.ts')
  assert.deepEqual(await api.getLatestReadings(), [])
  assert.equal(requestedUrl, 'http://configured.example.test:4500/custom-api/lecturas/ultima')
  assert.equal(requestedOptions.headers['X-API-Key'], 'test-api-key')
})

test('La instancia informa configuración ausente y no usa un host por defecto', async t => {
  const server = await environment(t, 'VITE_API_KEY=test-api-key\n')
  await assert.rejects(server.ssrLoadModule('/src/api/index.ts'), /Configura VITE_API_URL/)
})

test('La instancia permite mostrar el rechazo de la API cuando falta la clave', async t => {
  const server = await environment(t, 'VITE_API_URL=http://configured.example.test:4500/custom-api\n')
  const { api } = await server.ssrLoadModule('/src/api/index.ts')
  assert.equal(typeof api.getLatestReadings, 'function')
})
