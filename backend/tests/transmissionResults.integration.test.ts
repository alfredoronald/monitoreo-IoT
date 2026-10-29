import assert from 'node:assert/strict';
import test from 'node:test';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { importResults } from '../scripts/importTransmissionResults.js';

test('migra nueve resultados v1 sin duplicados ni pérdida de ids entre formatos', async () => {
  const schema = `identity152_${process.pid}`;
  const db = new Pool({ ...config.database, options: `-c search_path=${schema}` });
  const originalConfig = { ...config.database };
  const argv = process.argv;
  const output = process.env.TRANSMISSION_OUTPUT_DIR;
  const temp = await mkdtemp(join(tmpdir(), 'identity152-'));
  try {
    await db.query(`CREATE SCHEMA ${schema}`);
    const sql = await readFile(new URL('../../database/esquema.sql', import.meta.url), 'utf8');
    await db.query(sql.match(/CREATE TABLE IF NOT EXISTS pruebas_transmision[\s\S]*?\n\);/)![0]);
    await db.query(await readFile(new URL('../../database/001_extend_transmission_metrics.sql', import.meta.url), 'utf8'));
    const report = JSON.parse(await readFile(new URL('../../docs/evidencias/t9-6/resumen.json', import.meta.url), 'utf8'));
    for (const row of report.rows) {
      await db.query(`INSERT INTO pruebas_transmision
        (ejecucion_clave, descripcion, duracion_min, qos, intervalo_s, enviados, recibidos, guardados, metricas, observaciones)
        VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)`,
      [row.key, row.description, row.durationMinutes, row.qos, row.interval,
        row.sent, row.received, row.saved, row.metrics, JSON.stringify(row.metrics)]);
    }
    const snapshot = async () => (await db.query(`SELECT id, fecha_hora, duracion_min, qos, intervalo_s,
      enviados, recibidos, guardados, perdidos_pct, metricas FROM pruebas_transmision ORDER BY id`)).rows;
    const before = await snapshot();
    assert.equal(before.length, 9);
    Object.assign(config.database, { options: `-c search_path=${schema}` });
    process.env.TRANSMISSION_OUTPUT_DIR = join(temp, 'output');
    for (const format of ['lf', 'crlf', 'compact', 'reordered']) {
      const folder = join(temp, format);
      await mkdir(folder);
      const files: string[] = [];
      for (const [i, source] of report.sources.entries()) {
        const data = JSON.parse(await readFile(new URL(`../${source.file}`, import.meta.url), 'utf8'));
        let raw = JSON.stringify(data, null, 2) + '\n';
        if (format === 'crlf') raw = raw.replace(/\n/g, '\r\n');
        if (format === 'compact') raw = JSON.stringify(data);
        if (format === 'reordered') raw = JSON.stringify(Object.fromEntries(Object.entries(data).reverse()), null, 4);
        const file = join(folder, `${i}.json`);
        await writeFile(file, raw);
        files.push(file);
      }
      process.argv = ['node', 'import-results', '--apply', ...files];
      await importResults();
      assert.deepEqual(await snapshot(), before, `preserva ids y métricas (${format})`);
      const result = JSON.parse(await readFile(join(temp, 'output/resumen.json'), 'utf8'));
      assert.equal(result.complete, true);
      assert.equal(result.persistence.length, 9);
      assert.equal(result.identityVersion, 2);
    }
    // Si ya existen dos filas del mismo resultado, no escoger ni borrar una.
    const original = report.rows.find((row: any) => row.key.startsWith('149:'));
    await db.query(`INSERT INTO pruebas_transmision
      (ejecucion_clave, descripcion, enviados, recibidos, guardados, metricas)
      VALUES ($1,$2,$3,$4,$5,$6)`,
    [original.key, original.description, original.sent, original.received, original.saved, original.metrics]);
    const ambiguous = await snapshot();
    await assert.rejects(importResults(), /filas duplicadas/);
    assert.deepEqual(await snapshot(), ambiguous, 'revierte sin borrar ni fusionar filas');
  } finally {
    process.argv = argv;
    if (output === undefined) delete process.env.TRANSMISSION_OUTPUT_DIR;
    else process.env.TRANSMISSION_OUTPUT_DIR = output;
    Object.assign(config.database, originalConfig);
    delete (config.database as typeof config.database & { options?: string }).options;
    try { await db.query(`DROP SCHEMA IF EXISTS ${schema} CASCADE`); }
    finally { await db.end(); await rm(temp, { recursive: true, force: true }); }
  }
});
