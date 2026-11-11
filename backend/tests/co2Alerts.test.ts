import assert from 'node:assert/strict';
import test from 'node:test';
import type { Pool } from 'pg';
import { insertCo2Reading } from '../src/services/co2AlertService.js';

const thresholds = [
  { id: '3', nivel: 'malo', valor_desde: '1200' },
  { id: '2', nivel: 'moderado', valor_desde: '800' },
  { id: '1', nivel: 'bueno', valor_desde: '0' },
];

function database(previous?: string, stored = '850.00', fail?: string) {
  const calls: Array<{ text: string; values?: unknown[] }> = [];
  let released = false;
  const client = {
    async query(text: string, values?: unknown[]) {
      calls.push({ text, values });
      if (fail && text.includes(fail)) throw new Error('fallo simulado');
      if (text.startsWith('SELECT valor')) return { rows: previous === undefined ? [] : [{ valor: previous }] };
      if (text.startsWith('SELECT id')) return { rows: thresholds };
      if (text.startsWith('INSERT INTO lecturas')) return { rows: [{ id: '42', valor: stored }] };
      return { rows: [] };
    },
    release() { released = true; },
  };
  const pool = { connect: async () => client } as unknown as Pool;
  return { pool, calls, released: () => released };
}

for (const [previous, value, expected] of [
  [undefined, '700', false], [undefined, '850', true], [undefined, '1250', true],
  ['700', '850', true], ['700', '1250', true], ['850', '1250', true],
  ['1250', '950', true], ['850', '900', false], ['1250', '1300', false],
  ['950', '700', false], ['1250', '700', false],
  ['700', '800', true], ['850', '1200', true],
] as const) {
  test(`Transición ${previous ?? 'sin anterior'} → ${value}: alerta=${expected}`, async () => {
    const db = database(previous, value);
    await insertCo2Reading(db.pool, { tipo_variable: 'co2', valor: Number(value), fecha_hora: '2026-10-16T12:00:00Z' });
    const alerts = db.calls.filter(c => c.text.startsWith('INSERT INTO alertas'));
    assert.equal(alerts.length, Number(expected));
    if (expected) {
      assert.equal(alerts[0].values?.[0], '42');
      assert.equal(alerts[0].values?.[3], false);
    }
    assert.equal(db.calls[0].text, 'BEGIN ISOLATION LEVEL READ COMMITTED');
    assert.match(db.calls[1].text, /pg_advisory_xact_lock/);
    assert.equal(db.calls.at(-1)?.text, 'COMMIT');
    assert.equal(db.released(), true);
  });
}

test('Clasifica el valor redondeado por PostgreSQL, no el payload original', async () => {
  const db = database('700', '800.00');
  await insertCo2Reading(db.pool, { tipo_variable: 'co2', valor: 799.999, fecha_hora: '2026-10-16T12:00:00Z' });
  assert.equal(db.calls.filter(c => c.text.startsWith('INSERT INTO alertas')).length, 1);
});

for (const failure of ['INSERT INTO lecturas', 'INSERT INTO alertas']) {
  test(`Revierte y libera el cliente si falla ${failure}`, async () => {
    const db = database('700', '850', failure);
    await assert.rejects(insertCo2Reading(db.pool, { tipo_variable: 'co2', valor: 850, fecha_hora: '2026-10-16T12:00:00Z' }), /fallo simulado/);
    assert.equal(db.calls.at(-1)?.text, 'ROLLBACK');
    assert.equal(db.calls.some(c => c.text === 'COMMIT'), false);
    assert.equal(db.released(), true);
  });
}
