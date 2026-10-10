import assert from 'node:assert/strict';
import { once } from 'node:events';
import { createServer } from 'node:net';
import test from 'node:test';
import { startMqttSubscriber } from '../src/mqtt/mqttSubscriber.js';

test('El suscriptor solo genera JSON para mensajes de lectura válidos', { timeout: 5000 }, async (context) => {
  // Broker mínimo de prueba: CONNECT, SUBSCRIBE y PUBLISH con QoS 0.
  const broker = createServer((socket) => {
    let pending = Buffer.alloc(0);
    socket.on('data', (data) => {
      pending = Buffer.concat([pending, data]);
      while (pending.length >= 2 && pending.length >= pending[1] + 2) {
        const packet = pending.subarray(0, pending[1] + 2);
        pending = pending.subarray(packet.length);
        if (packet[0] === 0x10) socket.write(Buffer.from([0x20, 0x02, 0x00, 0x00]));
        if (packet[0] !== 0x82) continue;
        socket.write(Buffer.from([0x90, 0x03, packet[2], packet[3], 0x00]));
        for (const [topic, content] of [
          ['ambiente/temperatura', '24.5'], ['ambiente/humedad', 'abc'],
          ['ambiente/status', '1'], ['ambiente/presion', '1013'],
        ]) {
          const topicBuffer = Buffer.from(topic);
          const body = Buffer.concat([
            Buffer.from([0, topicBuffer.length]), topicBuffer, Buffer.from(content),
          ]);
          socket.write(Buffer.concat([Buffer.from([0x30, body.length]), body]));
        }
      }
    });
  });
  broker.listen(0, '127.0.0.1');
  await once(broker, 'listening');
  const address = broker.address();
  assert.ok(address && typeof address !== 'string');
  const output: string[] = [];
  const previousEnvironment = { ...process.env };
  process.env.MQTT_URL = `mqtt://127.0.0.1:${address.port}`;
  process.env.MQTT_USER = 'backend';
  process.env.MQTT_PASS = 'test-only';
  context.after(() => {
    process.env = previousEnvironment;
    broker.close();
  });
  context.mock.method(console, 'log', (message: string) => output.push(message));

  const client = startMqttSubscriber();
  context.after(() => client.end(true));
  await new Promise<void>((resolve) => {
    let received = 0;
    client.on('message', () => {
      received += 1;
      if (received === 4) resolve();
    });
  });

  const readings = output.filter((line) => line.startsWith('[MQTT] Lectura válida: '));
  assert.equal(readings.length, 1);
  const reading = JSON.parse(readings[0].slice('[MQTT] Lectura válida: '.length));
  assert.equal(reading.tipo_variable, 'temperatura');
  assert.equal(reading.valor, 24.5);
  assert.equal(output.filter((line) => line.startsWith('[MQTT] Descartado: ')).length, 3);
});
