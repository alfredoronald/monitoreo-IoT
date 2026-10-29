import assert from 'node:assert/strict';
import test from 'node:test';
import { readFile } from 'node:fs/promises';
import { normalizeEvidence } from '../scripts/importTransmissionResults.js';

test('QoS real conserva QoS efectivo 0 en ambas publicaciones y conteos', async () => {
  const data = JSON.parse(await readFile(new URL('../../docs/evidencias/t9-2/resultados.json', import.meta.url), 'utf8'));
  const rows = normalizeEvidence(data, 'ejecucion');
  assert.equal(rows.length, 2);
  assert.deepEqual(rows.map(row => [row.qos, row.metrics.qos_efectivo, row.sent, row.received, row.saved]),
    [[0,0,500,500,500], [1,0,500,500,500]]);
});
test('no convierte resultados incompletos en mediciones válidas', () => {
  assert.throws(() => normalizeEvidence({ issue:150, completed:false }, 'x'));
  assert.throws(() => normalizeEvidence({ issue:151, measurements:[] }, 'x'));
  assert.throws(() => normalizeEvidence({ issue:149, scenario:'wifi', seconds:20 }, 'x'));
  assert.throws(() => normalizeEvidence({ issue:999 }, 'x'));
});
test('continuidad de cinco minutos conserva duración y umbral del 98%', () => {
  const evidence = { issue: 150, completed: true, runId: 'five-minute-run', interval: 15,
    started: '2026-10-28T10:00:00Z', plannedEnd: '2026-10-28T10:05:00Z', incidents: [],
    counts: [{ variable: 'temperatura', saved: 20 }, { variable: 'humedad', saved: 19 },
      { variable: 'co2', saved: 21 }] };
  const rows = normalizeEvidence(evidence, 'five-minutes');
  assert.deepEqual(rows.map(row => [row.durationMinutes, row.sent, row.metrics.aprobado]),
    [[5, 20, true], [5, 20, false], [5, 20, false]]);
  assert.throws(() => normalizeEvidence({ ...evidence, plannedEnd: '2026-10-28T10:04:59Z' }, 'short'));
  assert.throws(() => normalizeEvidence({ ...evidence, plannedEnd: '2026-10-29T10:00:00Z' }, 'old-window'));
});
