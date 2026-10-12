import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, rmSync, rmdirSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { createMessageLogger, redactLogText } from '../src/mqtt/messageLogger.js';

test('Un contenido con saltos de línea produce un único registro de descarte', (context) => {
  const directory = mkdtempSync(join(tmpdir(), 'mqtt-logger-'));
  const filePath = join(directory, 'messages.jsonl');
  context.mock.method(console, 'log', () => undefined);
  context.after(() => { rmSync(filePath, { force: true }); rmdirSync(directory); });
  const logger = createMessageLogger(filePath);
  logger('received', 'ambiente/humedad', 'abc\nmensaje adicional');
  logger('discarded', 'ambiente/humedad', 'abc\nmensaje adicional', 'El contenido no es numérico');
  const lines = readFileSync(filePath, 'utf8').trim().split('\n');
  assert.equal(lines.length, 2);
  const entry = JSON.parse(lines[1]);
  assert.equal(entry.content, 'abc\nmensaje adicional');
  assert.equal(entry.topic, 'ambiente/humedad');
  assert.equal(entry.reason, 'El contenido no es numérico');
  assert.ok(Number.isFinite(Date.parse(entry.timestamp)));
});

test('Oculta secretos del entorno en el archivo y en la consola', (context) => {
  const directory = mkdtempSync(join(tmpdir(), 'mqtt-secret-test-'));
  const filePath = join(directory, 'messages.jsonl');
  const previous = process.env.MQTT_PASS;
  process.env.MQTT_PASS = 'private-mqtt-credential';
  context.after(() => {
    if (previous === undefined) delete process.env.MQTT_PASS;
    else process.env.MQTT_PASS = previous;
    rmSync(filePath, { force: true }); rmdirSync(directory);
  });
  const output: string[] = [];
  context.mock.method(console, 'log', (line: string) => output.push(line));
  createMessageLogger(filePath)(
    'discarded', 'ambiente/private-mqtt-credential',
    'private-mqtt-credential\nAPI_KEY=unknown-key\n{"password":"another-secret"}',
    'Motivo con private-mqtt-credential',
  );
  const written = readFileSync(filePath, 'utf8');
  for (const secret of ['private-mqtt-credential', 'unknown-key', 'another-secret']) {
    assert.ok(!written.includes(secret));
    assert.ok(!output.join('\n').includes(secret));
  }
  assert.ok(written.includes('[REDACTADO]'));
});

test('Oculta la contraseña de una URL de conexión', () => {
  const safe = redactLogText('Falló mqtt://backend:unknown-url-password@localhost:1883');
  assert.ok(!safe.includes('unknown-url-password'));
});

test('Un archivo de log no escribible no interrumpe el procesamiento', (context) => {
  const directory = mkdtempSync(join(tmpdir(), 'mqtt-write-failure-'));
  const blocker = join(directory, 'not-a-directory');
  writeFileSync(blocker, '');
  context.after(() => { rmSync(blocker); rmdirSync(directory); });
  const errors: string[] = [];
  context.mock.method(console, 'log', () => undefined);
  context.mock.method(console, 'error', (line: string) => errors.push(line));
  assert.doesNotThrow(() => createMessageLogger(join(blocker, 'messages.jsonl'))(
    'received', 'ambiente/temperatura', '25',
  ));
  assert.equal(errors.length, 1);
});
