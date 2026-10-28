import assert from 'node:assert/strict';
import test from 'node:test';
import { averageLatency } from '../scripts/latencyCheck.js';

test('promedio de diez medidas y límite estricto de 20 segundos', () => {
  assert.equal(averageLatency([1,2,3,4,5,6,7,8,9,10]), 5.5);
  assert.equal(averageLatency(Array(10).fill(20)) < 20, false);
  assert.throws(() => averageLatency([1,2]));
  assert.throws(() => averageLatency([...Array(9).fill(1), NaN]));
  assert.throws(() => averageLatency([...Array(9).fill(1), -1]));
});
