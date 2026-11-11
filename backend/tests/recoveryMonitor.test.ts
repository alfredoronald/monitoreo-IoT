import assert from 'node:assert/strict';
import test from 'node:test';
import { recoverySeconds } from '../scripts/recoveryMonitor.js';

test('el límite de recuperación es estricto y conserva milisegundos', () => {
  assert.equal(recoverySeconds(1000, 60999), 59.999);
  assert.equal(recoverySeconds(1000, 61000) < 60, false);
  assert.throws(() => recoverySeconds(2000, 1000));
  assert.throws(() => recoverySeconds(NaN, 1000));
});
