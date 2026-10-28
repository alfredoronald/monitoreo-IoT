import assert from 'node:assert/strict';
import test from 'node:test';
import { expectedReadings, percentage } from '../scripts/continuityRun.js';

test('mínimos enteros al 98% y ajuste del intervalo final', () => {
  assert.deepEqual(expectedReadings(15), { expected: 5760, minimum: 5645 });
  assert.deepEqual(expectedReadings(10), { expected: 8640, minimum: 8468 });
  assert.deepEqual(expectedReadings(30), { expected: 2880, minimum: 2823 });
  assert.ok(percentage(5645, 5760) >= 98);
  assert.ok(percentage(5644, 5760) < 98);
  for (const value of [0, 9, 31, NaN, 15.5]) assert.throws(() => expectedReadings(value));
});
