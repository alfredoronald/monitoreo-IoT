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
  const previous = process.env.VITE_API_URL
  delete process.env.VITE_API_URL
  const server = await createServer({
    root: fileURLToPath(new URL('..', import.meta.url)),
    configFile: false, envDir, mode: 'test', logLevel: 'silent',
    server: { middlewareMode: true, hmr: false },
  })
  t.after(async () => {
    await server.close()
    await rm(envDir, { recursive: true, force: true })
    if (previous === undefined) delete process.env.VITE_API_URL
    else process.env.VITE_API_URL = previous
  })
  return server
}

test('Vite carga VITE_API_URL del archivo .env y la instancia usa ese prefijo', async t => {
  const server = await environment(t, 'VITE_API_URL=http://configured.example.test:4500/custom-api\n')
  const original = globalThis.fetch
  let requestedUrl
  globalThis.fetch = async url => { requestedUrl = url; return Response.json([]) }
  t.after(() => { globalThis.fetch = original })
  const { api } = await server.ssrLoadModule('/src/api/index.ts')
  assert.deepEqual(await api.getLatestReadings(), [])
  assert.equal(requestedUrl, 'http://configured.example.test:4500/custom-api/lecturas/ultima')
})

test('La instancia informa configuración ausente y no usa un host por defecto', async t => {
  const server = await environment(t, '')
  await assert.rejects(server.ssrLoadModule('/src/api/index.ts'), /Configura VITE_API_URL/)
})
