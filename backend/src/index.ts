import dotenv from 'dotenv';
import { fileURLToPath } from 'node:url';
import { createApp } from './app.js';

// La ruta funciona tanto desde src/ como desde dist/.
dotenv.config({ path: fileURLToPath(new URL('../.env', import.meta.url)) });

const port = Number(process.env.API_PORT ?? 3000);
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('API_PORT debe ser un entero entre 1 y 65535');
}

// API_KEY se utilizará en el Sprint 3; todavía no condiciona el arranque.
const app = createApp(process.env.CORS_ORIGIN ?? 'http://localhost:5173');

const server = app.listen(port, () => {
  console.log(`Servidor de monitoreo ambiental disponible en http://localhost:${port}`);
});

server.on('error', (error) => {
  console.error('No se pudo iniciar el servidor:', error.message);
  process.exit(1);
});
