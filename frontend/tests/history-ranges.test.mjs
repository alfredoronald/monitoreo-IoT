import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import test from 'node:test'
import { quickHistoryRange, toLocalDateTime, validateCustomHistoryRange } from '../src/history/historyRanges.ts'

test('Los periodos rápidos duran exactamente 60 minutos o 24 horas y comparten un único instante actual', t => {
  let clockReads = 0
  const now = Date.parse('2026-10-16T00:30:15.123Z')
  t.mock.method(Date, 'now', () => { clockReads++; return now })
  for (const [period, duration] of [['last-hour', 3_600_000], ['last-24-hours', 86_400_000]]) {
    const range = quickHistoryRange(period)
    assert.equal(Date.parse(range.hasta), now)
    assert.equal(Date.parse(range.hasta) - Date.parse(range.desde), duration)
    assert.equal(range.desde.slice(0, 10), '2026-10-15')
  }
  assert.equal(clockReads, 2)
})

test('El rango personalizado convierte fechas del dispositivo a ISO UTC con segundos', () => {
  const result = validateCustomHistoryRange({ desde: '2026-10-15T08:30:12', hasta: '2026-10-15T09:45' })
  assert.deepEqual(result.errors, {})
  assert.equal(result.range.desde, new Date(2026, 9, 15, 8, 30, 12).toISOString())
  assert.equal(result.range.hasta, new Date(2026, 9, 15, 9, 45).toISOString())
  assert.equal(toLocalDateTime(result.range.desde), '2026-10-15T08:30:12')
})

test('Ambas fechas son obligatorias y los errores identifican cada campo', () => {
  const result = validateCustomHistoryRange({ desde: '', hasta: '' })
  assert.equal(result.range, null)
  assert.match(result.errors.desde, /Completa/)
  assert.match(result.errors.hasta, /Completa/)
})

test('Rechaza calendarios imposibles, horas inválidas y formatos ajenos a datetime-local', () => {
  for (const value of ['2026-02-29T12:00', '2026-02-30T12:00', '2026-04-31T12:00', '2026-13-01T12:00',
    '2026-10-00T12:00', '2026-10-15T24:00', '2026-10-15T12:60', '2026-10-15T12:00:60',
    '0000-01-01T12:00', '2026-10-15', '2026-10-15T12:00Z', ' ', 'invalid']) {
    const result = validateCustomHistoryRange({ desde: value, hasta: '2026-12-31T23:59:59' })
    assert.equal(result.range, null, value)
    assert.match(result.errors.desde, /existentes/, value)
  }
  assert.ok(validateCustomHistoryRange({ desde: '2028-02-29T12:00', hasta: '2028-03-01T12:00' }).range)
})

test('Rechaza fechas invertidas y permite exactamente el mismo instante en ambos extremos', () => {
  const reversed = validateCustomHistoryRange({ desde: '2026-10-16T12:00', hasta: '2026-10-15T12:00' })
  assert.equal(reversed.range, null)
  assert.match(reversed.errors.hasta, /igual o posterior/)
  const equal = validateCustomHistoryRange({ desde: '2026-10-15T12:00', hasta: '2026-10-15T12:00' })
  assert.deepEqual(equal.errors, {})
  assert.equal(equal.range.desde, equal.range.hasta)
})

test('La conversión respeta distintas zonas y rechaza horas inexistentes por cambio de horario', () => {
  const moduleUrl = new URL('../src/history/historyRanges.ts', import.meta.url).href
  const run = (zone, draft, periodNow) => JSON.parse(execFileSync(process.execPath,
    ['--experimental-strip-types', '--input-type=module', '-e', `
      import { validateCustomHistoryRange, quickHistoryRange, toLocalDateTime } from ${JSON.stringify(moduleUrl)};
      const result = validateCustomHistoryRange(${JSON.stringify(draft)});
      console.log(JSON.stringify({result, day: quickHistoryRange('last-24-hours', ${periodNow}),
        local: result.range ? toLocalDateTime(result.range.desde) : null}));
    `], { encoding: 'utf8', env: { ...process.env, TZ: zone } }))
  const draft = { desde: '2026-10-15T08:00', hasta: '2026-10-15T09:00' }
  const now = Date.parse('2026-03-09T01:30:00-04:00')
  assert.equal(run('America/La_Paz', draft, now).result.range.desde, '2026-10-15T12:00:00.000Z')
  assert.equal(run('Asia/Kolkata', draft, now).result.range.desde, '2026-10-15T02:30:00.000Z')
  const dst = run('America/New_York', { desde: '2026-03-08T02:30', hasta: '2026-03-08T04:00' }, now)
  assert.equal(dst.result.range, null)
  assert.match(dst.result.errors.desde, /existentes/)
  assert.equal(Date.parse(dst.day.hasta) - Date.parse(dst.day.desde), 86_400_000)
})
