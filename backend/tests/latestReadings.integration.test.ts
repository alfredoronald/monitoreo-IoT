import assert from 'node:assert/strict';
import { once } from 'node:events';
import { readFile } from 'node:fs/promises';
import type { Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import test from 'node:test';
import type { PoolClient } from 'pg';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';

test('Últimas lecturas mediante HTTP y la vista PostgreSQL real', async (context) => {
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
  const schema = `issue76_${process.pid}`;
  try {
    await database.query(`CREATE SCHEMA ${schema}`);
    await database.query(`SET LOCAL search_path TO ${schema}`);
  } catch (error) {
    if ((error as { code?: string }).code !== '42501') throw error;
    await database.query('ROLLBACK TO SAVEPOINT create_test_schema');
    await database.query('CREATE TEMP TABLE issue76_bootstrap (id integer)');
    await database.query('SET LOCAL search_path TO pg_temp');
  }
  await database.query('RELEASE SAVEPOINT create_test_schema');
  await database.query(await readFile(new URL('../../database/esquema.sql', import.meta.url), 'utf8'));
  context.mock.method(pool, 'query', async (text: string, values?: unknown[]) => {
    assert.match(text, /FROM\s+v_ultima_lectura\b/i);
    return database.query(text, values);
  });

  server = createApp('http://localhost:5173', 'test-only-key').listen(0, '127.0.0.1');
  await once(server, 'listening');
  const endpoint = `http://127.0.0.1:${(server.address() as AddressInfo).port}/api/lecturas/ultima`;
  const readLatest = async () => {
    const response = await fetch(endpoint, { headers: { 'X-API-Key': 'test-only-key' } });
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /^application\/json/);
    return response.json();
  };

  await context.test('Sin lecturas devuelve un array vacío', async () => {
    assert.deepEqual(await readLatest(), []);
  });

  const expected = [
    { tipo_variable: 'co2', valor: 0, fecha_hora: '2026-10-15T12:00:15.000Z' },
    { tipo_variable: 'humedad', valor: 55.25, fecha_hora: '2026-10-15T12:00:14.000Z' },
    { tipo_variable: 'temperatura', valor: 24.75, fecha_hora: '2026-10-15T12:00:15.000Z' },
  ];
  await context.test('Selecciona por fecha y conserva números, fechas UTC y campos del contrato', async () => {
    await database.query(`INSERT INTO lecturas (fecha_hora, tipo_variable, valor) VALUES
      ('2026-10-15T08:00:15-04:00', 'co2', 0),
      ('2026-10-15T11:59:00Z', 'co2', 800),
      ('2026-10-15T12:00:15Z', 'temperatura', 24.75),
      ('2026-10-15T12:00:00Z', 'temperatura', 20),
      ('2026-10-15T08:00:14-04:00', 'humedad', 55.25),
      ('2026-10-15T11:58:00Z', 'humedad', 50)`);
    assert.deepEqual(await readLatest(), expected);
  });

  await context.test('Omite una variable ausente sin inventar valores', async () => {
    await database.query("DELETE FROM lecturas WHERE tipo_variable = 'humedad'");
    assert.deepEqual(await readLatest(), [expected[0], expected[2]]);
  });
});
