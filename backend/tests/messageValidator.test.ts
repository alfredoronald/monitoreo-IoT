import assert from 'node:assert/strict';
import test from 'node:test';
import { validateMessage } from '../src/mqtt/messageValidator.js';

test('Construye el contrato de lectura para las tres variables', () => {
  for (const variable of ['temperatura', 'humedad', 'co2']) {
    const before = Date.now();
    const result = validateMessage(`ambiente/${variable}`, '24.5');
    const after = Date.now();

    assert.equal(result.valid, true);
    if (!result.valid) assert.fail(result.reason);
    assert.deepEqual(Object.keys(result.reading).sort(), ['fecha_hora', 'tipo_variable', 'valor']);
    assert.equal(result.reading.tipo_variable, variable);
    assert.equal(result.reading.valor, 24.5);
    const timestamp = Date.parse(result.reading.fecha_hora);
    assert.ok(timestamp >= before && timestamp <= after);
    assert.equal(new Date(timestamp).toISOString(), result.reading.fecha_hora);
  }
});

test('Acepta cero, negativos, decimales y notación científica', () => {
  for (const [content, expected] of [
    ['0', 0], ['-12.5', -12.5], ['  +24.5  ', 24.5], ['.5', 0.5], ['1e3', 1000],
  ] as const) {
    const result = validateMessage('ambiente/temperatura', content);
    if (!result.valid) assert.fail(result.reason);
    assert.equal(result.reading.valor, expected);
  }
});

test('Descarta topics desconocidos sin producir una lectura', () => {
  for (const topic of [
    '', 'ambiente/presion', 'otro/temperatura', 'ambiente/dispositivo/temperatura',
    'ambiente/temperatura/extra', 'ambiente/Temperatura', 'ambiente/toString',
  ]) {
    const result = validateMessage(topic, '24.5');
    assert.equal(result.valid, false, topic);
    assert.equal('reading' in result, false, topic);
  }
});

test('Descarta el estado del dispositivo aunque contenga un número', () => {
  for (const content of ['online', 'offline', '1']) {
    const result = validateMessage('ambiente/status', content);
    assert.equal(result.valid, false);
    assert.equal('reading' in result, false);
  }
});

test('Descarta contenidos vacíos, no numéricos, parciales y no finitos', () => {
  for (const content of [
    '', '   ', 'abc', '25abc', '24.5 °C', '24,5', 'NaN', 'Infinity', '-Infinity',
    '1e309', '0x10', 'true', 'null', '{"valor":24.5}', '1 2',
  ]) {
    const result = validateMessage('ambiente/temperatura', content);
    assert.equal(result.valid, false, content);
    assert.equal('reading' in result, false, content);
  }
});
