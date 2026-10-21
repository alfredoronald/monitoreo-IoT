import assert from 'node:assert/strict';
import { once } from 'node:events';
import { readFile } from 'node:fs/promises';
import type { Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import { performance } from 'node:perf_hooks';
import test from 'node:test';
import type { PoolClient } from 'pg';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';

interface CaseResult {
  id: string;
  description: string;
  expected: string;
  observed: string;
  durationMs: number | null;
  status: 'PASS' | 'FAIL';
}

interface HttpResult {
  status: number;
  body: unknown;
  headers: Headers;
  durationMs: number;
}

const dashboardOrigin = 'http://localhost:5173';
const rangeStart = '2026-10-15T12:00:00Z';
const rangeEnd = '2026-10-15T13:00:00Z';
const dayStart = '2026-10-19T00:00:00Z';
const dayEnd = '2026-10-20T00:00:00Z';

function historyPath(variable = 'temperatura', from = rangeStart, to = rangeEnd) {
  return `/api/lecturas?${new URLSearchParams({ variable, desde: from, hasta: to })}`;
}

function rowsOf(result: HttpResult): Array<Record<string, unknown>> {
  assert.ok(Array.isArray(result.body), 'Se esperaba un array JSON.');
  return result.body as Array<Record<string, unknown>>;
}

function errorMessage(result: HttpResult): string {
  assert.ok(result.body && typeof result.body === 'object' && !Array.isArray(result.body));
  const message = (result.body as { mensaje?: unknown }).mensaje;
  assert.equal(typeof message, 'string');
  assert.ok((message as string).length > 0);
  return `mensaje: ${message}`;
}

test('T-4.8: casos HTTP y consulta de 24 h contra PostgreSQL aislado', async (context) => {
  let client: PoolClient | undefined;
  let server: Server | undefined;
  const cases: CaseResult[] = [];

  context.after(async () => {
    try {
      if (server?.listening) {
        await new Promise<void>((resolve, reject) => {
          server!.close((error) => error ? reject(error) : resolve());
        });
      }
    } finally {
      try {
        if (client) await client.query('ROLLBACK');
      } finally {
        client?.release();
        await pool.end();
      }
    }
  });

  const database = client = await pool.connect();
  await database.query('BEGIN');
  await database.query('SAVEPOINT create_test_schema');
  const schema = `issue78_${process.pid}`;
  try {
    await database.query(`CREATE SCHEMA ${schema}`);
    await database.query(`SET LOCAL search_path TO ${schema}`);
  } catch (error) {
    if ((error as { code?: string }).code !== '42501') throw error;
    await database.query('ROLLBACK TO SAVEPOINT create_test_schema');
    await database.query('CREATE TEMP TABLE issue78_bootstrap (id integer)');
    await database.query('SET LOCAL search_path TO pg_temp');
  }
  await database.query('RELEASE SAVEPOINT create_test_schema');
  await database.query(await readFile(new URL('../../database/esquema.sql', import.meta.url), 'utf8'));

  // La API usa el mismo cliente transaccional. Ninguna lectura de prueba se confirma.
  context.mock.method(pool, 'query', async (sql: string, values?: unknown[]) => database.query(sql, values));
  server = createApp(dashboardOrigin).listen(0, '127.0.0.1');
  await once(server, 'listening');
  const baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;

  async function call(path: string, init?: RequestInit): Promise<HttpResult> {
    const started = performance.now();
    const response = await fetch(`${baseUrl}${path}`, init);
    const body = await response.json();
    return {
      status: response.status,
      body,
      headers: response.headers,
      durationMs: Number((performance.now() - started).toFixed(3)),
    };
  }

  async function check(
    id: string,
    description: string,
    expected: string,
    path: string,
    expectedStatus: number,
    verify: (result: HttpResult) => string,
    init?: RequestInit,
  ) {
    let result: HttpResult | undefined;
    try {
      result = await call(path, init);
      assert.equal(result.status, expectedStatus);
      assert.match(result.headers.get('content-type') ?? '', /^application\/json/);
      const detail = verify(result);
      cases.push({ id, description, expected,
        observed: `HTTP ${result.status}; ${detail}`, durationMs: result.durationMs, status: 'PASS' });
    } catch (error) {
      const detail = error instanceof Error ? error.message : String(error);
      cases.push({ id, description, expected,
        observed: `${result ? `HTTP ${result.status}; ` : ''}${detail}`,
        durationMs: result?.durationMs ?? null, status: 'FAIL' });
    }
  }

  await check('H-01', 'Histórico sin lecturas', '200; []', historyPath(), 200, (result) => {
    assert.equal(rowsOf(result).length, 0);
    return '0 lecturas';
  });
  await check('U-01', 'Últimas lecturas sin datos', '200; []', '/api/lecturas/ultima', 200, (result) => {
    assert.equal(rowsOf(result).length, 0);
    return '0 lecturas';
  });

  await database.query(`INSERT INTO lecturas (id, fecha_hora, tipo_variable, valor)
    OVERRIDING SYSTEM VALUE VALUES
    (101, '2026-10-15T12:00:00Z', 'temperatura', 23.75),
    (102, '2026-10-15T13:00:00Z', 'temperatura', 24.50),
    (103, '2026-10-15T11:59:59Z', 'temperatura', 20.00),
    (104, '2026-10-15T12:30:00Z', 'humedad', 55.25),
    (105, '2026-10-15T12:45:00Z', 'co2', 800.50)`);

  await check('H-02', 'Histórico válido con extremos incluidos', '200; IDs 101 y 102 en orden',
    historyPath(), 200, (result) => {
      const rows = rowsOf(result);
      assert.deepEqual(rows.map((row) => row.id), ['101', '102']);
      assert.deepEqual(rows.map((row) => row.valor), [23.75, 24.5]);
      assert.deepEqual(rows.map((row) => row.fecha_hora),
        ['2026-10-15T12:00:00.000Z', '2026-10-15T13:00:00.000Z']);
      return '2 lecturas, orden y formato correctos';
    });
  await check('H-03', 'Rango válido sin coincidencias', '200; []',
    historyPath('co2', '2026-10-14T00:00:00Z', '2026-10-14T01:00:00Z'), 200,
    (result) => { assert.equal(rowsOf(result).length, 0); return '0 lecturas'; });
  await check('H-04', 'Zona horaria equivalente', '200; 2 lecturas',
    historyPath('temperatura', '2026-10-15T08:00:00-04:00', '2026-10-15T09:00:00-04:00'),
    200, (result) => { assert.equal(rowsOf(result).length, 2); return '2 lecturas'; });
  await check('U-02', 'Última lectura de cada variable', '200; co2, humedad y temperatura',
    '/api/lecturas/ultima', 200, (result) => {
      const rows = rowsOf(result);
      assert.deepEqual(rows.map((row) => row.tipo_variable), ['co2', 'humedad', 'temperatura']);
      assert.ok(rows.every((row) => !('id' in row) && typeof row.valor === 'number'
        && typeof row.fecha_hora === 'string'));
      assert.equal(rows[2].valor, 24.5);
      return '3 variables, fecha y valor válidos';
    });

  for (const [id, description, path] of [
    ['H-05', 'Variable desconocida', historyPath('ruido')],
    ['H-06', 'Falta desde', `/api/lecturas?${new URLSearchParams({ variable: 'temperatura', hasta: rangeEnd })}`],
    ['H-07', 'Fecha imposible', historyPath('temperatura', '2026-02-30T12:00:00Z', rangeEnd)],
    ['H-08', 'Rango invertido', historyPath('temperatura', rangeEnd, rangeStart)],
    ['H-09', 'Parámetro duplicado', `${historyPath()}&variable=humedad`],
    ['H-10', 'Parámetro desconocido', `${historyPath()}&limite=1`],
    ['U-03', 'Parámetros no admitidos en última', '/api/lecturas/ultima?variable=co2'],
  ] as const) {
    await check(id, description, '400; mensaje en español', path, 400, errorMessage);
  }

  await check('C-01', 'Origen autorizado por CORS', '200; Access-Control-Allow-Origin exacto',
    historyPath(), 200, (result) => {
      assert.equal(result.headers.get('access-control-allow-origin'), dashboardOrigin);
      assert.equal(rowsOf(result).length, 2);
      return 'origen permitido';
    }, { headers: { Origin: dashboardOrigin } });
  await check('C-02', 'Origen rechazado por CORS', '403; sin cabecera de permiso',
    historyPath(), 403, (result) => {
      assert.equal(result.headers.get('access-control-allow-origin'), null);
      return errorMessage(result);
    }, { headers: { Origin: 'http://localhost:5174' } });

  // Exactamente 5760 muestras de una variable, una cada 15 s en 24 h.
  await database.query(`INSERT INTO lecturas (id, fecha_hora, tipo_variable, valor)
    OVERRIDING SYSTEM VALUE
    SELECT 10000 + g, $1::timestamptz + (g * interval '15 seconds'), 'temperatura', 22.50
    FROM generate_series(1, 5760) AS g`, [dayStart]);
  const dayPath = historyPath('temperatura', dayStart, dayEnd);
  function verifyDay(result: HttpResult) {
    const rows = rowsOf(result);
    assert.equal(rows.length, 5760);
    assert.equal(rows[0].fecha_hora, '2026-10-19T00:00:15.000Z');
    assert.equal(rows.at(-1)?.fecha_hora, '2026-10-20T00:00:00.000Z');
    return '5760 lecturas de temperatura, ventana de 24 h';
  }
  await check('P-00', 'Preparación de la consulta de 24 h', '200; 5760 lecturas',
    dayPath, 200, verifyDay);
  for (let index = 1; index <= 3; index++) {
    await check(`P-0${index}`, `Consulta de 24 h, medición ${index}`, '200; 5760 lecturas; <1000 ms',
      dayPath, 200, (result) => {
        const detail = verifyDay(result);
        assert.ok(result.durationMs < 1000, `Tiempo ${result.durationMs} ms, límite 1000 ms.`);
        return detail;
      });
  }

  const times = cases.filter((item) => /^P-0[1-3]$/.test(item.id))
    .map((item) => item.durationMs).filter((value): value is number => value !== null);
  const report = {
    issue: 78,
    timestampUtc: new Date().toISOString(),
    environment: { node: process.version, postgres: (await database.query('SHOW server_version')).rows[0].server_version },
    dataset: { variable: 'temperatura', from: dayStart, to: dayEnd, intervalSeconds: 15,
      count: 5760, rolledBackAfterTest: true },
    dailyQueryTimesMs: times,
    cases,
    passed: cases.filter((item) => item.status === 'PASS').length,
    failed: cases.filter((item) => item.status === 'FAIL').length,
  };
  console.log(`EVIDENCIA_T4_8=${JSON.stringify(report)}`);
  assert.equal(report.failed, 0, `${report.failed} casos fallaron; revisar EVIDENCIA_T4_8.`);
});
