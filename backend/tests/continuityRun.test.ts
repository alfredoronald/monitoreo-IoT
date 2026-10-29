import assert from 'node:assert/strict';
import test from 'node:test';
import { expectedReadings, percentage } from '../scripts/continuityRun.js';

test('mínimos enteros al 98% y ajuste del intervalo final', () => {
  assert.deepEqual(expectedReadings(15), { expected: 20, minimum: 20 });
  assert.deepEqual(expectedReadings(10), { expected: 30, minimum: 30 });
  assert.deepEqual(expectedReadings(30), { expected: 10, minimum: 10 });
  assert.ok(percentage(20, 20) >= 98);
  assert.ok(percentage(19, 20) < 98);
  for (const value of [0, 9, 31, NaN, 15.5]) assert.throws(() => expectedReadings(value));
});
