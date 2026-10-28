import assert from 'node:assert/strict';
import { once } from 'node:events';
import { readFile } from 'node:fs/promises';
import type { Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import test from 'node:test';
import type { PoolClient } from 'pg';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';

test('Histórico mediante HTTP y PostgreSQL real, aislado de los datos existentes', async (context) => {
  let client: PoolClient | undefined;
  let server: Server | undefined;
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
  const schema = `issue74_${process.pid}`;
  try {
    await database.query(`CREATE SCHEMA ${schema}`);
    await database.query(`SET LOCAL search_path TO ${schema}`);
  } catch (error) {
    if ((error as { code?: string }).code !== '42501') throw error;
    await database.query('ROLLBACK TO SAVEPOINT create_test_schema');
    await database.query('CREATE TEMP TABLE issue74_bootstrap (id integer)');
    await database.query('SET LOCAL search_path TO pg_temp');
  }
  await database.query('RELEASE SAVEPOINT create_test_schema');
  await database.query(await readFile(new URL('../../database/esquema.sql', import.meta.url), 'utf8'));
  context.mock.method(pool, 'query', async (text: string, values?: unknown[]) => {
    assert.match(text, /FROM\s+lecturas\b/i);
    assert.match(text, /\$1/);
    assert.match(text, /\$2/);
    assert.match(text, /\$3/);
    assert.equal(values?.length, 3);
    return database.query(text, values);
  });

  server = createApp('http://localhost:5173', 'test-only-key').listen(0, '127.0.0.1');
  await once(server, 'listening');
  const endpoint = `http://127.0.0.1:${(server.address() as AddressInfo).port}/api/lecturas`;
  const readHistory = async (
    variable: string, desde = '2026-10-15T12:00:00Z', hasta = '2026-10-15T13:00:00Z',
  ) => {
    const response = await fetch(`${endpoint}?${new URLSearchParams({ variable, desde, hasta })}`, { headers: { 'X-API-Key': 'test-only-key' } });
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /^application\/json/);
    return response.json();
  };

  await context.test('Sin lecturas devuelve 200 con una lista vacía', async () => {
    assert.deepEqual(await readHistory('temperatura'), []);
  });
  await database.query(`INSERT INTO lecturas (id, fecha_hora, tipo_variable, valor)
    OVERRIDING SYSTEM VALUE VALUES
    (9007199254740995, '2026-10-15T13:00:00Z', 'temperatura', 25.50),
    (9007199254740994, '2026-10-15T12:00:00Z', 'temperatura', 23.75),
    (9007199254740993, '2026-10-15T12:00:00Z', 'temperatura', 0),
    (9007199254740996, '2026-10-15T11:59:59.999Z', 'temperatura', 19),
    (9007199254740997, '2026-10-15T13:00:00.001Z', 'temperatura', 27),
    (11, '2026-10-15T12:30:00Z', 'humedad', 55.25),
    (12, '2026-10-15T12:40:00Z', 'co2', 800.50),
    (13, '2026-10-15T13:00:00Z', 'co2', 0)`);
  const temperature = [
    { id: '9007199254740993', tipo_variable: 'temperatura', valor: 0, fecha_hora: '2026-10-15T12:00:00.000Z' },
    { id: '9007199254740994', tipo_variable: 'temperatura', valor: 23.75, fecha_hora: '2026-10-15T12:00:00.000Z' },
    { id: '9007199254740995', tipo_variable: 'temperatura', valor: 25.5, fecha_hora: '2026-10-15T13:00:00.000Z' },
  ];
  await context.test('Filtra, incluye los extremos y ordena empates sin perder precisión de BIGINT', async () => {
    assert.deepEqual(await readHistory('temperatura'), temperature);
  });
  await context.test('Conserva decimales, cero y el filtro de cada variable', async () => {
    assert.deepEqual(await readHistory('humedad'), [
      { id: '11', tipo_variable: 'humedad', valor: 55.25, fecha_hora: '2026-10-15T12:30:00.000Z' },
    ]);
    assert.deepEqual(await readHistory('co2'), [
      { id: '12', tipo_variable: 'co2', valor: 800.5, fecha_hora: '2026-10-15T12:40:00.000Z' },
      { id: '13', tipo_variable: 'co2', valor: 0, fecha_hora: '2026-10-15T13:00:00.000Z' },
    ]);
  });
  await context.test('Interpreta offsets equivalentes y permite desde igual a hasta', async () => {
    assert.deepEqual(await readHistory('temperatura', '2026-10-15T08:00:00-04:00', '2026-10-15T09:00:00-04:00'), temperature);
    assert.deepEqual(await readHistory('temperatura', '2026-10-15T12:00:00Z', '2026-10-15T12:00:00Z'), temperature.slice(0, 2));
  });
  await context.test('Con datos fuera del rango devuelve una lista vacía', async () => {
    assert.deepEqual(await readHistory('co2', '2026-10-14T12:00:00Z', '2026-10-14T13:00:00Z'), []);
  });
  await context.test('Entrega todas las coincidencias sin truncar a 5760 registros', async () => {
    await database.query(`INSERT INTO lecturas (id, fecha_hora, tipo_variable, valor) OVERRIDING SYSTEM VALUE
      SELECT 100 + g, '2026-10-16T12:00:00Z'::timestamptz + (g * interval '1 millisecond'), 'co2', 400
      FROM generate_series(0, 5760) AS g`);
    const rows = await readHistory('co2', '2026-10-16T12:00:00Z', '2026-10-16T13:00:00Z');
    assert.ok(Array.isArray(rows));
    assert.equal(rows.length, 5761);
  });
});
