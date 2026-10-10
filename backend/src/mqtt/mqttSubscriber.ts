import mqtt from 'mqtt';

export function startMqttSubscriber() {
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
    console.log(`[MQTT] ${topic}: ${payload.toString('utf8')}`);
  });

  client.on('reconnect', () => {
    console.log('[MQTT] Intentando reconectar al broker');
  });

  client.on('offline', () => {
    console.warn('[MQTT] Broker desconectado; se reintentará la conexión');
  });

  client.on('error', (error) => {
    console.error('[MQTT] Error de conexión:', error.message);
  });

  return client;
}
