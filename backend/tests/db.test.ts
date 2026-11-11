import assert from 'node:assert/strict';
import test from 'node:test';
import { insertReading, pool } from '../src/db.js';

test('Inserta una lectura con parámetros PostgreSQL', async (context) => {
  let queryText = '';
  let queryValues: unknown[] = [];

  context.mock.method(pool, 'query', async (text: string, values: unknown[]) => {
    queryText = text;
    queryValues = values;
    return { rows: [], command: 'INSERT', rowCount: 1 };
  });

  const reading = {
    fecha_hora: '2026-10-12T04:00:00.000Z',
    tipo_variable: 'temperatura' as const,
    valor: 24.5,
  };
  await insertReading(reading);

  assert.match(queryText, /INSERT INTO lecturas \(fecha_hora, tipo_variable, valor\)/);
  assert.match(queryText, /VALUES \(\$1, \$2, \$3\)/);
  assert.deepEqual(queryValues, [reading.fecha_hora, reading.tipo_variable, reading.valor]);
});
