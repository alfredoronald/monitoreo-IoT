import assert from 'node:assert/strict';
import { request, type IncomingHttpHeaders, type Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import { after, before, test } from 'node:test';
import express from 'express';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';
import { errorHandler } from '../src/middleware/errorHandler.js';

const origin = 'http://localhost:5173';
let server: Server;
let port: number;
before(async () => {
  await new Promise<void>((resolve) => { server = createApp(origin, 'test-only-key').listen(0, '127.0.0.1', resolve); });
  port = (server.address() as AddressInfo).port;
});
after(async () => {
  await new Promise<void>((resolve) => server.close(() => resolve()));
  await pool.end();
});

function call(targetPort: number, path: string, headers: Record<string, string> = {}, method = 'GET', body?: string) {
  return new Promise<{ status: number; headers: IncomingHttpHeaders; text: string }>((resolve, reject) => {
    const client = request({ host: '127.0.0.1', port: targetPort, path, method, headers: {
      'X-API-Key': 'test-only-key', ...headers, ...(body === undefined ? {} : { 'Content-Length': String(Buffer.byteLength(body)) }),
    } }, (response) => {
      let text = '';
      response.setEncoding('utf8');
      response.on('data', (part: string) => { text += part; });
      response.on('error', reject);
      response.on('end', () => resolve({ status: response.statusCode!, headers: response.headers, text }));
    });
    client.on('error', reject);
    client.end(body);
  });
}

test('Permite el origen exacto del dashboard y mantiene acceso sin Origin', async () => {
  const allowed = await call(port, '/', { Origin: origin });
  assert.equal(allowed.status, 200);
  assert.equal(allowed.headers['access-control-allow-origin'], origin);
  assert.match(String(allowed.headers.vary), /Origin/);
  assert.equal(allowed.headers['access-control-allow-credentials'], undefined);
  const cli = await call(port, '/');
  assert.equal(cli.status, 200);
  assert.equal(cli.headers['access-control-allow-origin'], undefined);
});

for (const denied of ['http://localhost:5174', 'http://127.0.0.1:5173', 'https://localhost:5173',
  'http://localhost:5173.ejemplo.com', 'null']) {
  test(`Rechaza origen ${denied} antes de consultar BD`, async (context) => {
    const query = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const result = await call(port, '/api/lecturas/ultima', { Origin: denied });
    assert.equal(result.status, 403);
    assert.deepEqual(JSON.parse(result.text), { mensaje: 'Origen no permitido por CORS.' });
    assert.equal(result.headers['access-control-allow-origin'], undefined);
    assert.equal(query.mock.callCount(), 0);
  });
}

test('El origen configurado no está fijado al puerto de desarrollo', async () => {
  const custom = createApp('https://dashboard.example.com', 'test-only-key').listen(0, '127.0.0.1');
  await new Promise<void>((resolve) => custom.once('listening', resolve));
  try {
    const result = await call((custom.address() as AddressInfo).port, '/', { Origin: 'https://dashboard.example.com' });
    assert.equal(result.headers['access-control-allow-origin'], 'https://dashboard.example.com');
    assert.equal((await call((custom.address() as AddressInfo).port, '/', { Origin: origin })).status, 403);
  } finally { await new Promise<void>((resolve) => custom.close(() => resolve())); }
});

test('Preflight permite PATCH y Content-Type solo para el origen configurado', async () => {
  const headers = { Origin: origin, 'Access-Control-Request-Method': 'PATCH', 'Access-Control-Request-Headers': 'content-type' };
  const result = await call(port, '/api/alertas/7', headers, 'OPTIONS');
  assert.equal(result.status, 204);
  assert.equal(result.headers['access-control-allow-origin'], origin);
  assert.equal(result.headers['access-control-allow-methods'], 'GET,PATCH,OPTIONS');
  assert.equal(result.headers['access-control-allow-headers'], 'Content-Type,X-API-Key');
  const denied = await call(port, '/api/alertas/7', { ...headers, Origin: 'http://otro.example.com' }, 'OPTIONS');
  assert.equal(denied.status, 403);
  assert.equal(denied.headers['access-control-allow-origin'], undefined);
});

for (const path of ['/api/lecturas/ultima',
  '/api/lecturas?variable=co2&desde=2026-10-15T12:00:00Z&hasta=2026-10-15T13:00:00Z',
  '/api/estado-aire', '/api/alertas?atendida=false', '/api/alertas/7']) {
  test(`Centraliza un fallo inesperado en ${path}`, async (context) => {
    context.mock.method(pool, 'query', async () => { throw new Error('SELECT password=secreto host=privado'); });
    const patch = path === '/api/alertas/7';
    const result = await call(port, path, { Origin: origin, 'Content-Type': 'application/json' },
      patch ? 'PATCH' : 'GET', patch ? '{"atendida":true}' : undefined);
    assert.equal(result.status, 500);
    assert.equal(result.headers['access-control-allow-origin'], origin);
    assert.deepEqual(JSON.parse(result.text), { mensaje: 'No se pudo completar la solicitud.' });
    assert.doesNotMatch(result.text, /SELECT|secreto|privado|stack|Error:/);
  });
}

test('JSON incorrecto conserva 400 y un cuerpo demasiado grande responde 413 uniforme', async (context) => {
  const query = context.mock.method(pool, 'query', async () => ({ rows: [] }));
  for (const [body, status, mensaje] of [
    ['{', 400, 'El cuerpo no contiene JSON válido.'],
    [JSON.stringify({ dato: 'a'.repeat(110000) }), 413, 'El cuerpo de la solicitud supera el tamaño permitido.'],
  ] as const) {
    const result = await call(port, '/api/alertas/7', { Origin: origin, 'Content-Type': 'application/json' }, 'PATCH', body);
    assert.equal(result.status, status);
    assert.deepEqual(JSON.parse(result.text), { mensaje });
  }
  assert.equal(query.mock.callCount(), 0);
});

test('Una excepción síncrona no controlada llega al manejador sin revelar detalles', async () => {
  const app = express();
  app.get('/', () => { throw new Error('password=secreto'); });
  app.use(errorHandler);
  const failing = app.listen(0, '127.0.0.1');
  await new Promise<void>((resolve) => failing.once('listening', resolve));
  try {
    const result = await call((failing.address() as AddressInfo).port, '/');
    assert.equal(result.status, 500);
    assert.deepEqual(JSON.parse(result.text), { mensaje: 'No se pudo completar la solicitud.' });
  } finally { await new Promise<void>((resolve) => failing.close(() => resolve())); }
});
