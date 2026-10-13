import { createApp } from './app.js';
import { startMqttSubscriber } from './mqtt/mqttSubscriber.js';
import { config } from './config.js';
import { connectDatabase, pool } from './db.js';

const port = config.apiPort;
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('API_PORT debe ser un entero entre 1 y 65535');
}

// API_KEY se utilizará en el Sprint 3; todavía no condiciona el arranque.
const mqttClient = startMqttSubscriber();
try {
  await connectDatabase();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`No se pudo conectar a PostgreSQL. El backend no iniciará: ${message}`);
  mqttClient.end(true);
  await pool.end().catch(() => undefined);
  process.exit(1);
}

const app = createApp(config.corsOrigin);

const server = app.listen(port, () => {
  console.log(`Servidor de monitoreo ambiental disponible en http://localhost:${port}`);
});

server.on('error', (error) => {
  console.error('No se pudo iniciar el servidor:', error.message);
  mqttClient.end(true);
  process.exit(1);
});

function shutdown() {
  console.log('Cerrando servidor y conexión MQTT');
  mqttClient.end(true);
  server.close();
  void pool.end().catch((error: unknown) => {
    const message = error instanceof Error ? error.message : String(error);
    console.error('No se pudo cerrar la conexión PostgreSQL:', message);
  });
}

process.once('SIGINT', shutdown);
process.once('SIGTERM', shutdown);
