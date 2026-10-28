import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import dotenv from 'dotenv';
import pg from 'pg';

dotenv.config({ path: fileURLToPath(new URL('../.env', import.meta.url)), quiet: true });

test('Registro de calibración en esquema temporal aislado', async t => {
  const schema = `qa_calibration_${Date.now()}_${process.pid}`;
  assert.match(schema, /^qa_calibration_\d+_\d+$/);
  const client = new pg.Client({
    host: process.env.PG_HOST, port: Number(process.env.PG_PORT),
    database: process.env.PG_DB, user: process.env.PG_USER,
    password: process.env.PG_PASS, connectionTimeoutMillis: 5000,
  });
  await client.connect();
  const qualified = `"${schema}".calibraciones`;
  const migration = (await readFile(new URL('../../database/001_add_calibration_sample_count.sql', import.meta.url), 'utf8'))
    .replaceAll('public.calibraciones', qualified);
  const registration = (await readFile(new URL('../../database/register_calibration.psql.sql', import.meta.url), 'utf8'))
    .replaceAll('public.calibraciones', qualified);
  // Datos sintéticos de la prueba: nunca se insertan en public.calibraciones.
  const fixture = { sensor: 'MQ-135', ro_kohm: '50.123', numero_lecturas: '60',
    co2_referencia_ppm: '420', acondicionamiento_horas: '0.25',
    fecha_hora: '2026-10-28T16:20:00-04:00', observaciones: "Prueba sintética; 'comillas'; no calibración física" };
  const render = values => registration.replace(/:'(\w+)'/g, (_, key) => {
    assert.ok(key in values);
    return `'${String(values[key]).replaceAll("'", "''")}'`;
  });
  const rows = async () => (await client.query(`SELECT * FROM ${qualified} ORDER BY id`)).rows;
  try {
    await client.query(`CREATE SCHEMA "${schema}"`);
    await client.query(`CREATE TABLE ${qualified} (
      id integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
      sensor varchar(20) NOT NULL, ro_kohm numeric(10,3) NOT NULL CHECK(ro_kohm>0),
      co2_referencia_ppm numeric(8,2) NOT NULL, acondicionamiento_horas numeric(6,1),
      fecha_hora timestamptz NOT NULL, vigente boolean NOT NULL, observaciones text
    ); CREATE UNIQUE INDEX uq_calibracion_vigente ON ${qualified}(sensor) WHERE vigente;
    INSERT INTO ${qualified}(sensor,ro_kohm,co2_referencia_ppm,fecha_hora,vigente)
      VALUES ('MQ-135',40,400,now(),true),('otro-sensor',45,400,now(),true);`);

    await t.test('Migración repetible conserva filas y cantidades históricas desconocidas', async () => {
      await client.query(migration); await client.query(migration);
      const result = await rows();
      assert.equal(result.length, 2);
      assert.ok(result.every(row => row.numero_lecturas === null));
      await assert.rejects(client.query(`UPDATE ${qualified} SET numero_lecturas=-1`), { code: '23514' });
    });
    await t.test('Activa una referencia y conserva historial, otro sensor y literales', async () => {
      await client.query(render(fixture));
      const result = await rows();
      assert.equal(result.length, 3);
      assert.equal(result[0].vigente, false);
      assert.equal(result[1].vigente, true);
      assert.equal(result[2].ro_kohm, '50.123');
      assert.equal(result[2].numero_lecturas, 60);
      assert.equal(result[2].acondicionamiento_horas, '0.250');
      assert.equal(result[2].observaciones, fixture.observaciones);
      assert.equal(result.filter(row => row.sensor === 'MQ-135' && row.vigente).length, 1);
    });
    await t.test('Entradas inválidas no sustituyen la referencia vigente', async () => {
      for (const invalid of [{ numero_lecturas: '49' }, { ro_kohm: '0' },
        { ro_kohm: '0.0001' }, { co2_referencia_ppm: '-1' },
        { acondicionamiento_horas: '0.1' }, { observaciones: '  ' },
        { ro_kohm: 'NaN' }, { co2_referencia_ppm: 'NaN' },
        { acondicionamiento_horas: 'NaN' }]) {
        await assert.rejects(client.query(render({ ...fixture, ...invalid })));
        await client.query('ROLLBACK');
        const result = await rows();
        assert.equal(result.length, 3);
        assert.equal(result[2].vigente, true);
      }
    });
    await t.test('Fallo de inserción revierte la desactivación de la calibración previa', async () => {
      await client.query(`ALTER TABLE ${qualified} ADD CONSTRAINT qa_reject_new CHECK(ro_kohm<60)`);
      await assert.rejects(client.query(render({ ...fixture, ro_kohm: '70' })));
      await client.query('ROLLBACK');
      const result = await rows();
      assert.equal(result.length, 3);
      assert.equal(result[2].vigente, true);
    });
  } finally {
    await client.query('ROLLBACK').catch(() => {});
    await client.query(`DROP SCHEMA IF EXISTS "${schema}" CASCADE`);
    await client.end();
  }
});
