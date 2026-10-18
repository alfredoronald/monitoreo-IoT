import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { insertCo2Reading } from '../src/services/co2AlertService.js';

test('Alertas CO2 con PostgreSQL real, aisladas del histórico habitual', { timeout: 30000 }, async context => {
  const schema = `issue90_${process.pid}`;
  const admin = new Pool(config.database);
  const db = new Pool({ ...config.database, options: `-c search_path=${schema}`, max: 10 });
  context.after(async () => {
    await db.end();
    try { await admin.query(`DROP SCHEMA IF EXISTS ${schema} CASCADE`); }
    finally { await admin.end(); }
  });
  await admin.query(`CREATE SCHEMA ${schema}`);
  await db.query(await readFile(new URL('../../database/esquema.sql', import.meta.url), 'utf8'));
  const save = (value: number, target = db) => insertCo2Reading(target, {
    tipo_variable: 'co2', valor: value, fecha_hora: new Date().toISOString(),
  });
  const clear = () => db.query('TRUNCATE alertas, lecturas RESTART IDENTITY CASCADE');

  await context.test('Ocho lecturas producen cuatro alertas nuevas sin atender', async () => {
    for (const value of [700, 850, 900, 1250, 1300, 950, 700, 850]) await save(value);
    const { rows } = await db.query(`SELECT u.nivel, l.valor, a.atendida
      FROM alertas a JOIN lecturas l ON l.id = a.lectura_id
      JOIN umbrales u ON u.id = a.umbral_id ORDER BY a.id`);
    assert.deepEqual(rows.map(r => [r.nivel, Number(r.valor), r.atendida]), [
      ['moderado', 850, false], ['malo', 1250, false],
      ['moderado', 950, false], ['moderado', 850, false],
    ]);
    assert.equal(Number((await db.query('SELECT count(*) FROM lecturas')).rows[0].count), 8);
  });

  await context.test('Primera detección moderada o mala genera una única alerta', async () => {
    for (const value of [850, 1250]) {
      await clear();
      await save(value);
      await save(value);
      assert.equal(Number((await db.query('SELECT count(*) FROM alertas')).rows[0].count), 1);
    }
  });

  await context.test('Conexiones concurrentes y nueva instancia no duplican nivel', async () => {
    await clear();
    await save(700);
    await Promise.all(Array.from({ length: 12 }, () => save(850)));
    const other = new Pool({ ...config.database, options: `-c search_path=${schema}` });
    try { await save(900, other); } finally { await other.end(); }
    assert.equal(Number((await db.query('SELECT count(*) FROM alertas')).rows[0].count), 1);
    assert.equal(Number((await db.query('SELECT count(*) FROM lecturas')).rows[0].count), 14);
  });

  await context.test('Umbrales editables y redondeo NUMERIC determinan la clasificación', async () => {
    await clear();
    await db.query("UPDATE umbrales SET valor_desde = 900 WHERE tipo_variable = 'co2' AND nivel = 'moderado'");
    await save(850);
    assert.equal(Number((await db.query('SELECT count(*) FROM alertas')).rows[0].count), 0);
    await save(899.999);
    const { rows } = await db.query('SELECT l.valor FROM alertas a JOIN lecturas l ON l.id = a.lectura_id');
    assert.deepEqual(rows.map(r => r.valor), ['900.00']);
  });

  await context.test('Una alerta fallida revierte la lectura y permite el siguiente guardado', async () => {
    await clear();
    await save(700);
    await db.query('ALTER TABLE alertas ADD CONSTRAINT reject_test_alert CHECK (false)');
    await assert.rejects(save(950));
    assert.equal(Number((await db.query('SELECT count(*) FROM lecturas')).rows[0].count), 1);
    await db.query('ALTER TABLE alertas DROP CONSTRAINT reject_test_alert');
    await save(950);
    assert.equal(Number((await db.query('SELECT count(*) FROM alertas')).rows[0].count), 1);
  });
});
