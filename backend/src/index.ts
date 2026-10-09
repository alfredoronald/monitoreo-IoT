import { createApp } from './app.js';
import { config } from './config.js';
import { connectDatabase, pool } from './db.js';

// La ruta funciona tanto desde src/ como desde dist/.
const port = config.apiPort;
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('API_PORT debe ser un entero entre 1 y 65535');
}

// API_KEY se utilizará en el Sprint 3; todavía no condiciona el arranque.
try {
  await connectDatabase();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`No se pudo conectar a PostgreSQL. El backend no iniciará: ${message}`);
  await pool.end().catch(() => undefined);
  process.exit(1);
}

const app = createApp(config.corsOrigin);

const server = app.listen(port, () => {
  console.log(`Servidor de monitoreo ambiental disponible en http://localhost:${port}`);
});

server.on('error', (error) => {
  console.error('No se pudo iniciar el servidor:', error.message);
  process.exit(1);
});
