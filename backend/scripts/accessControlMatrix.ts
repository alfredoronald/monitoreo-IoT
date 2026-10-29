import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import dotenv from 'dotenv';
import mqtt, { type MqttClient } from 'mqtt';

dotenv.config({ path: fileURLToPath(new URL('../.env', import.meta.url)) });

const mqttUrl = process.env.MQTT_URL ?? 'mqtt://localhost:1883';
const marker = `issue134-${Date.now()}`;
const configPath = fileURLToPath(new URL('../../firmware/include/config.h', import.meta.url));

function configValue(config: string, name: string) {
  const match = config.match(new RegExp(`constexpr\\s+char\\s+${name}\\[\\]\\s*=\\s*"([^"]+)"`));
  if (!match) throw new Error(`No se encontró ${name} en firmware/include/config.h.`);
  return match[1];
}

function connect(options: mqtt.IClientOptions) {
  return new Promise<MqttClient>((resolve, reject) => {
    const client = mqtt.connect(mqttUrl, { ...options, connectTimeout: 5_000, reconnectPeriod: 0 });
    const timeout = setTimeout(() => {
      client.end(true);
      reject(new Error('Tiempo de conexión agotado.'));
    }, 6_000);
    client.once('connect', () => {
      clearTimeout(timeout);
      resolve(client);
    });
    client.once('error', error => {
      clearTimeout(timeout);
      client.end(true);
      reject(error);
    });
  });
}

function rejected(options: mqtt.IClientOptions) {
  return connect(options).then(
    client => {
      client.end(true);
      return false;
    },
    () => true,
  );
}

function subscribe(client: MqttClient, topic: string) {
  return new Promise<void>((resolve, reject) => {
    client.subscribe(topic, { qos: 1 }, error => error ? reject(error) : resolve());
  });
}

function publish(client: MqttClient, topic: string, payload: string) {
  return new Promise<{ denied: boolean; reasonCode?: number }>((resolve, reject) => {
    let reasonCode: number | undefined;
    const timeout = setTimeout(() => {
      client.off('packetreceive', packetListener);
      reject(new Error(`Tiempo de publicación agotado para ${topic}.`));
    }, 6_000);
    const packetListener = (packet: mqtt.Packet) => {
      if (packet.cmd === 'puback') reasonCode = packet.reasonCode;
    };
    client.on('packetreceive', packetListener);
    client.publish(topic, payload, { qos: 1 }, error => {
      clearTimeout(timeout);
      client.off('packetreceive', packetListener);
      if (error) {
        resolve({ denied: true, reasonCode: (error as Error & { code?: number }).code });
        return;
      }
      resolve({ denied: reasonCode === 135, reasonCode });
    });
  });
}

async function main() {
  const firmwareConfig = await readFile(configPath, 'utf8');
  const esp32 = { username: configValue(firmwareConfig, 'mqtt_user'), password: configValue(firmwareConfig, 'mqtt_password'), protocolVersion: 5 as const };
  const backend = { username: process.env.MQTT_USER, password: process.env.MQTT_PASS, protocolVersion: 5 as const };
  if (!backend.username || !backend.password) throw new Error('Faltan MQTT_USER o MQTT_PASS en backend/.env.');

  const results: Record<string, boolean | number | undefined> = {
    anonymousRejected: await rejected({ protocolVersion: 5 }),
    invalidCredentialsRejected: await rejected({ username: 'invalid-user', password: 'invalid-password', protocolVersion: 5 }),
  };
  const esp32Client = await connect(esp32);
  const backendClient = await connect(backend);
  let backendReceived = false;
  let esp32Received = false;
  const allowedTopic = `ambiente/${marker}`;
  const payload = marker;
  backendClient.on('message', (topic, message) => { if (topic === allowedTopic && message.toString() === payload) backendReceived = true; });
  esp32Client.on('message', (topic, message) => { if (topic === allowedTopic && message.toString() === payload) esp32Received = true; });

  try {
    await subscribe(backendClient, 'ambiente/#');
    await subscribe(esp32Client, 'ambiente/#');
    const allowed = await publish(esp32Client, allowedTopic, payload);
    await new Promise(resolve => setTimeout(resolve, 1_000));
    const esp32Outside = await publish(esp32Client, `fuera/${marker}`, payload);
    const backendWrite = await publish(backendClient, `ambiente/${marker}-backend`, payload);
    Object.assign(results, {
      esp32AllowedPublish: !allowed.denied && backendReceived,
      esp32ReadDenied: !esp32Received,
      esp32OutsideDenied: esp32Outside.denied,
      esp32OutsideReasonCode: esp32Outside.reasonCode,
      backendWriteDenied: backendWrite.denied,
      backendWriteReasonCode: backendWrite.reasonCode,
      backendReadAllowed: backendReceived,
    });
  } finally {
    esp32Client.end(true);
    backendClient.end(true);
  }

  console.log(JSON.stringify({ marker, ...results }, null, 2));
  if (Object.values(results).some(result => result === false)) process.exitCode = 1;
}

void main().catch(error => {
  console.error(error instanceof Error ? error.message : error);
  process.exitCode = 1;
});
