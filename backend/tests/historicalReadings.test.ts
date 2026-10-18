import assert from 'node:assert/strict';
import { request, type Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import { after, before, test } from 'node:test';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';
import { getHistoricalReadings } from '../src/services/historicalReadingService.js';

let server: Server;
let port: number;
const filters = { variable: 'temperatura', from: '2026-10-15T08:00:00-04:00', to: '2026-10-15T13:00:00Z' };
const query = new URLSearchParams({ variable: filters.variable, desde: filters.from, hasta: filters.to });

for (const [parameter, value, message] of [
  ['variable', 'ruido', 'variable debe ser'],
  ['variable', 'Temperatura', 'variable debe ser'],
  ['desde', 'ayer', 'desde debe ser'],
  ['hasta', '2026-10-16', 'hasta debe ser'],
  ['desde', '2026-10-15T12:00:00', 'desde debe ser'],
  ['desde', '2026-02-30T12:00:00Z', 'desde debe ser'],
  ['desde', '2026-02-29T12:00:00Z', 'desde debe ser'],
  ['desde', '1900-02-29T12:00:00Z', 'desde debe ser'],
  ['hasta', '2026-13-01T12:00:00Z', 'hasta debe ser'],
  ['hasta', '2026-04-31T12:00:00Z', 'hasta debe ser'],
  ['desde', '2026-10-00T12:00:00Z', 'desde debe ser'],
  ['desde', '2026-10-15T24:00:00Z', 'desde debe ser'],
  ['desde', '2026-10-15T12:60:00Z', 'desde debe ser'],
  ['desde', '2026-10-15T12:00:60Z', 'desde debe ser'],
  ['desde', '2026-10-15T12:00:00+16:00', 'desde debe ser'],
  ['desde', '2026-10-15T12:00:00+04:60', 'desde debe ser'],
  ['desde', '2026-10-16T12:00:00Z', 'desde no puede ser posterior'],
]) {
  test(`Validación HTTP rechaza ${parameter}=${value} sin consultar BD`, async (context) => {
    const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const invalidQuery = new URLSearchParams(query);
    invalidQuery.set(parameter, value);
    const result = await get(invalidQuery.toString());
    assert.equal(result.status, 400);
    assert.match(result.contentType, /^application\/json/);
    assert.ok((result.body as { mensaje: string }).mensaje.startsWith(message));
    assert.equal(databaseQuery.mock.callCount(), 0);
  });
}

for (const parameter of ['variable', 'desde', 'hasta']) {
  test(`Explica que falta ${parameter}`, async (context) => {
    const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const incomplete = new URLSearchParams(query);
    incomplete.delete(parameter);
    const result = await get(incomplete.toString());
    assert.equal(result.status, 400);
    assert.deepEqual(result.body, { mensaje: `El parámetro ${parameter} es obligatorio.` });
    assert.equal(databaseQuery.mock.callCount(), 0);
  });
}

for (const [from, to, status] of [
  ['2024-02-29T00:00:00Z', '2024-02-29T00:00:00Z', 200],
  ['2000-02-29T00:00:00Z', '2000-02-29T01:00:00Z', 200],
  ['2026-10-15T08:00:00-04:00', '2026-10-15T12:00:00Z', 200],
  ['2026-10-15T14:00:00+02:00', '2026-10-15T12:00:00Z', 200],
  ['2026-10-15T08:00:00-05:00', '2026-10-15T12:00:00Z', 400],
  ['2026-10-15T12:00:00.123400Z', '2026-10-15T12:00:00.1234Z', 200],
  ['2026-10-15T12:00:00.1235Z', '2026-10-15T12:00:00.1234Z', 400],
] as const) {
  test(`Compara instantes y fracciones: ${from} a ${to}`, async (context) => {
    const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const result = await get(new URLSearchParams({ variable: 'co2', desde: from, hasta: to }).toString());
    assert.equal(result.status, status);
    assert.equal(databaseQuery.mock.callCount(), status === 200 ? 1 : 0);
    if (status === 200) assert.deepEqual(result.body, []);
  });
}

before(async () => {
  await new Promise<void>((resolve) => { server = createApp('http://localhost:5173').listen(0, '127.0.0.1', resolve); });
  port = (server.address() as AddressInfo).port;
});
after(async () => {
  await new Promise<void>((resolve) => server.close(() => resolve()));
  await pool.end();
});

function get(search = query.toString(), body?: string) {
  return new Promise<{ status: number; contentType: string; body: unknown }>((resolve, reject) => {
    const call = request({ host: '127.0.0.1', port, path: `/api/lecturas?${search}`, method: 'GET', agent: false,
      headers: body === undefined ? {} : { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) },
    }, (response) => {
      let content = '';
      response.setEncoding('utf8');
      response.on('data', (part: string) => { content += part; });
      response.on('error', reject);
      response.on('end', () => {
        try { resolve({ status: response.statusCode!, contentType: response.headers['content-type'] ?? '', body: JSON.parse(content) }); }
        catch (error) { reject(error); }
      });
    });
    call.on('error', reject);
    call.end(body);
  });
}

test('HTTP entrega el contrato conservando BIGINT, cero, decimales y fechas UTC', async (context) => {
  const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [
    { id: '9007199254740993', tipo_variable: 'temperatura', valor: '0.00', fecha_hora: new Date('2026-10-15T08:00:00-04:00') },
    { id: '9007199254740994', tipo_variable: 'temperatura', valor: '24.75', fecha_hora: new Date('2026-10-15T13:00:00Z') },
  ] }));
  const result = await get();
  assert.equal(result.status, 200);
  assert.match(result.contentType, /^application\/json/);
  assert.deepEqual(result.body, [
    { id: '9007199254740993', tipo_variable: 'temperatura', valor: 0, fecha_hora: '2026-10-15T12:00:00.000Z' },
    { id: '9007199254740994', tipo_variable: 'temperatura', valor: 24.75, fecha_hora: '2026-10-15T13:00:00.000Z' },
  ]);
  assert.deepEqual(databaseQuery.mock.calls[0].arguments[1], [filters.variable, filters.from, filters.to]);
});

test('Los valores enviados al servicio solo aparecen en parámetros SQL', async (context) => {
  const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
  const value = "temperatura' OR TRUE; DROP TABLE lecturas; --";
  await getHistoricalReadings({ ...filters, variable: value });
  const [sql, parameters] = databaseQuery.mock.calls[0].arguments;
  assert.match(String(sql), /tipo_variable = \$1/);
  assert.match(String(sql), /fecha_hora >= \$2::timestamptz/);
  assert.match(String(sql), /fecha_hora <= \$3::timestamptz/);
  assert.match(String(sql), /ORDER BY fecha_hora ASC, id ASC/);
  assert.ok(!String(sql).includes(value));
  assert.deepEqual(parameters, [value, filters.from, filters.to]);
});

test('Sin coincidencias devuelve HTTP 200 y una lista vacía', async (context) => {
  context.mock.method(pool, 'query', async () => ({ rows: [] }));
  const result = await get();
  assert.equal(result.status, 200);
  assert.deepEqual(result.body, []);
});

for (const search of ['', 'variable=co2', `${query}&variable=humedad`, `${query}&limite=1`,
  query.toString().replace('variable=temperatura', 'variable='), `${query}&desde%5Bx%5D=1`]) {
  test(`Rechaza parámetros ausentes, vacíos, repetidos o adicionales: ${search}`, async (context) => {
    const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    const result = await get(search);
    assert.equal(result.status, 400);
    assert.match(result.contentType, /^application\/json/);
    assert.equal(typeof (result.body as { mensaje: unknown }).mensaje, 'string');
    assert.equal(databaseQuery.mock.callCount(), 0);
  });
}

for (const body of ['{}', '{']) {
  test(`Rechaza un cuerpo GET antes del parser JSON: ${body}`, async (context) => {
    const databaseQuery = context.mock.method(pool, 'query', async () => ({ rows: [] }));
    assert.equal((await get(query.toString(), body)).status, 400);
    assert.equal(databaseQuery.mock.callCount(), 0);
  });
}

test('Un fallo PostgreSQL responde 500 uniforme sin detalles internos', async (context) => {
  context.mock.method(pool, 'query', async () => { throw new Error('SELECT secreto password=privado'); });
  const result = await get();
  assert.equal(result.status, 500);
  assert.deepEqual(result.body, { mensaje: 'No se pudo completar la solicitud.' });
});

for (const value of ['NaN', 'Infinity']) {
  test(`Un valor no finito ${value} no se presenta como null JSON`, async (context) => {
    context.mock.method(pool, 'query', async () => ({ rows: [
      { id: '1', tipo_variable: 'co2', valor: value, fecha_hora: new Date() },
    ] }));
    assert.equal((await get()).status, 500);
  });
}
