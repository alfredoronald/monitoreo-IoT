import assert from 'node:assert/strict';
import { request, type Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import { after, before, test } from 'node:test';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';

let server: Server;
let port: number;

before(async () => {
  await new Promise<void>((resolve) => {
    server = createApp('http://localhost:5173', 'test-only-key').listen(0, '127.0.0.1', resolve);
  });
  port = (server.address() as AddressInfo).port;
});

after(async () => {
  await new Promise<void>((resolve) => server.close(() => resolve()));
  await pool.end();
});

function get(path = '/api/lecturas/ultima', body?: string, chunked = false) {
  return new Promise<{ status: number; contentType: string; body: unknown }>((resolve, reject) => {
    const headers: Record<string, string | number> = { 'X-API-Key': 'test-only-key' };
    if (body !== undefined) {
      headers['Content-Type'] = body === 'texto' ? 'text/plain' : 'application/json';
      headers[chunked ? 'Transfer-Encoding' : 'Content-Length'] = chunked
        ? 'chunked' : Buffer.byteLength(body);
    }
    const call = request({ host: '127.0.0.1', port, path, method: 'GET', headers, agent: false }, (response) => {
      let content = '';
      response.setEncoding('utf8');
      response.on('data', (part: string) => { content += part; });
      response.on('error', reject);
      response.on('end', () => {
        try {
          resolve({
            status: response.statusCode!,
            contentType: response.headers['content-type'] ?? '',
            body: JSON.parse(content),
          });
        } catch (error) { reject(error); }
      });
    });
    call.on('error', reject);
    call.end(body);
  });
}

test('Consulta la vista y devuelve números y fechas UTC sin id ni envoltorio', async (context) => {
  const query = context.mock.method(pool, 'query', async () => ({ rows: [
    { id: '999', tipo_variable: 'co2', valor: '900.00', fecha_hora: new Date('2026-10-15T08:00:15-04:00') },
    { tipo_variable: 'humedad', valor: '0.00', fecha_hora: new Date('2026-10-15T08:00:14-04:00') },
    { tipo_variable: 'temperatura', valor: '24.70', fecha_hora: new Date('2026-10-15T08:00:15-04:00') },
  ] }));
  const result = await get();
  assert.equal(result.status, 200);
  assert.match(result.contentType, /^application\/json/);
  assert.deepEqual(result.body, [
    { tipo_variable: 'co2', valor: 900, fecha_hora: '2026-10-15T12:00:15.000Z' },
    { tipo_variable: 'humedad', valor: 0, fecha_hora: '2026-10-15T12:00:14.000Z' },
    { tipo_variable: 'temperatura', valor: 24.7, fecha_hora: '2026-10-15T12:00:15.000Z' },
  ]);
  assert.match(String(query.mock.calls[0].arguments[0]), /FROM v_ultima_lectura\s+ORDER BY tipo_variable ASC/);
});

test('Devuelve 200 y [] cuando no hay lecturas', async (context) => {
  context.mock.method(pool, 'query', async () => ({ rows: [] }));
  const result = await get();
  assert.equal(result.status, 200);
  assert.deepEqual(result.body, []);
});

test('Omite variables ausentes y conserva lecturas antiguas', async (context) => {
  context.mock.method(pool, 'query', async () => ({ rows: [
    { tipo_variable: 'co2', valor: '600.00', fecha_hora: new Date('2020-01-01T00:00:00Z') },
  ] }));
  const result = await get();
  assert.equal(result.status, 200);
  assert.deepEqual(result.body, [
    { tipo_variable: 'co2', valor: 600, fecha_hora: '2020-01-01T00:00:00.000Z' },
  ]);
});

for (const suffix of ['?variable=co2', '?x=', '?x=1&x=2', '?__proto__[x]=1']) {
  test(`Rechaza query no permitida ${suffix} antes de consultar la base`, async (context) => {
    const query = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const result = await get(`/api/lecturas/ultima${suffix}`);
    assert.equal(result.status, 400);
    assert.match(result.contentType, /^application\/json/);
    assert.deepEqual(result.body, { mensaje: 'Esta ruta no admite parámetros de consulta.' });
    assert.equal(query.mock.callCount(), 0);
  });
}

for (const body of ['{}', '{', 'texto']) {
  test(`Rechaza cuerpo no permitido ${body} antes de consultar la base`, async (context) => {
    const query = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const result = await get('/api/lecturas/ultima', body);
    assert.equal(result.status, 400);
    assert.deepEqual(result.body, { mensaje: 'Esta ruta no admite cuerpo en la solicitud.' });
    assert.equal(query.mock.callCount(), 0);
  });
}

test('Rechaza cuerpo enviado con transferencia fragmentada', async (context) => {
  const query = context.mock.method(pool, 'query', async () => ({ rows: [] }));
  assert.equal((await get('/api/lecturas/ultima', '{}', true)).status, 400);
  assert.equal(query.mock.callCount(), 0);
});

test('Un fallo PostgreSQL devuelve 500 sin detalles internos', async (context) => {
  context.mock.method(pool, 'query', async () => { throw new Error('SELECT secreto password=privado'); });
  const result = await get();
  assert.equal(result.status, 500);
  assert.deepEqual(result.body, { mensaje: 'No se pudo completar la solicitud.' });
});

for (const value of ['NaN', 'Infinity', '-Infinity']) {
  test(`Un valor no finito ${value} no se convierte en null JSON`, async (context) => {
    context.mock.method(pool, 'query', async () => ({ rows: [
      { tipo_variable: 'co2', valor: value, fecha_hora: new Date() },
    ] }));
    assert.equal((await get()).status, 500);
  });
}

test('Una fecha inválida no produce una lectura válida', async (context) => {
  context.mock.method(pool, 'query', async () => ({ rows: [
    { tipo_variable: 'co2', valor: '600.00', fecha_hora: new Date('inválida') },
  ] }));
  assert.equal((await get()).status, 500);
});
