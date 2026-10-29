import assert from 'node:assert/strict';
import test from 'node:test';
import { readFile } from 'node:fs/promises';
import { evidenceIdentity, normalizeEvidence } from '../scripts/importTransmissionResults.js';

test('QoS real conserva QoS efectivo 0 en ambas publicaciones y conteos', async () => {
  const data = JSON.parse(await readFile(new URL('../../docs/evidencias/t9-2/resultados.json', import.meta.url), 'utf8'));
  const rows = normalizeEvidence(data, 'ejecucion');
  assert.equal(rows.length, 2);
  assert.deepEqual(rows.map(row => [row.qos, row.metrics.qos_efectivo, row.sent, row.received, row.saved]),
    [[0,0,500,500,500], [1,0,500,500,500]]);
});
test('la identidad conserva ensayos entre LF, CRLF, formato y orden de propiedades', async () => {
  const raw = await readFile(new URL('../../docs/evidencias/t9-6/fuentes/149-broker.json', import.meta.url), 'utf8');
  const data = JSON.parse(raw);
  const variants = [raw.replace(/\r\n/g, '\n'), raw.replace(/\r?\n/g, '\r\n'), JSON.stringify(data),
    JSON.stringify(Object.fromEntries(Object.entries(data).reverse()), null, 4)];
  const expected = evidenceIdentity(raw);
  for (const variant of variants) {
    const actual = evidenceIdentity(variant);
    assert.equal(actual.identity, expected.identity);
    assert.equal(actual.canonicalSha256, expected.canonicalSha256);
    assert.deepEqual(normalizeEvidence(actual.data, actual.identity), normalizeEvidence(expected.data, expected.identity));
  }
  assert.notEqual(evidenceIdentity(JSON.stringify({ ...data, seconds: data.seconds + 1 })).identity, expected.identity);
  assert.notEqual(evidenceIdentity(JSON.stringify({ ...data, started: '2026-10-30T15:25:40.941Z' })).identity, expected.identity);
});
test('continuidad conserva su runId existente independientemente del formato', () => {
  const data = { issue: 150, runId: 'existing-run' };
  assert.equal(evidenceIdentity(JSON.stringify(data)).identity, 'existing-run');
  assert.equal(evidenceIdentity(JSON.stringify(data, null, 2).replace(/\n/g, '\r\n')).identity, 'existing-run');
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
