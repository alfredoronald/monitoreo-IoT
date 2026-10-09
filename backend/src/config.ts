import dotenv from 'dotenv';
import { fileURLToPath } from 'node:url';

// La ruta funciona tanto desde src/ como desde dist/.
dotenv.config({ path: fileURLToPath(new URL('../.env', import.meta.url)) });

export const config = {
  apiPort: Number(process.env.API_PORT ?? 3000),
  corsOrigin: process.env.CORS_ORIGIN ?? 'http://localhost:5173',
  database: {
    host: process.env.PG_HOST ?? '',
    port: Number(process.env.PG_PORT ?? 5432),
    database: process.env.PG_DB ?? '',
    user: process.env.PG_USER ?? '',
    password: process.env.PG_PASS ?? '',
  },
};
