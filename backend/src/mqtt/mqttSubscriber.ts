import mqtt from 'mqtt';
import { insertReading } from '../db.js';
import { validateMessage } from './messageValidator.js';
import { createMessageLogger, redactLogText } from './messageLogger.js';

export interface ReadingRepository {
  insertReading(reading: Parameters<typeof insertReading>[0]): Promise<void>;
}

export function startMqttSubscriber(
  repository: ReadingRepository = { insertReading },
  messageLogger = createMessageLogger(),
) {
  const { MQTT_URL, MQTT_USER, MQTT_PASS } = process.env;
  if (!MQTT_URL || !MQTT_USER || !MQTT_PASS) {
    throw new Error('Completa MQTT_URL, MQTT_USER y MQTT_PASS en backend/.env');
  }

  const client = mqtt.connect(MQTT_URL, {
    username: MQTT_USER,
    password: MQTT_PASS,
    clean: true,
    reconnectPeriod: 2000,
    reconnectOnConnackError: true,
    // La suscripción se realiza en cada evento connect, incluida la reconexión.
    resubscribe: false,
  });

  client.on('connect', () => {
    console.log('[MQTT] Conectado al broker');
    client.subscribe('ambiente/#', { qos: 0 }, (error, granted) => {
      if (error || !granted?.length || granted.some((subscription) => subscription.qos === 128)) {
        console.error('[MQTT] No se pudo suscribir a ambiente/#');
        return;
      }
      console.log('[MQTT] Suscrito a ambiente/#');
    });
  });

  client.on('message', (topic, payload) => {
    const content = payload.toString('utf8');
    messageLogger('received', topic, content);
    const result = validateMessage(topic, content);
    if (!result.valid) {
      messageLogger('discarded', topic, content, result.reason);
      return;
    }

    console.log(`[MQTT] Lectura válida: ${redactLogText(JSON.stringify(result.reading))}`);
    void repository.insertReading(result.reading).catch((error: unknown) => {
      const message = error instanceof Error ? error.message : String(error);
      console.error('[DB] No se pudo guardar la lectura:', redactLogText(message));
    });
  });

  client.on('reconnect', () => {
    console.log('[MQTT] Intentando reconectar al broker');
  });

  client.on('offline', () => {
    console.warn('[MQTT] Broker desconectado; se reintentará la conexión');
  });

  client.on('error', (error) => {
    console.error('[MQTT] Error de conexión:', redactLogText(error.message));
  });

  return client;
}
