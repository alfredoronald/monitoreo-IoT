import { Pool } from 'pg';
import { config } from './config.js';

function validateDatabaseConfig() {
  const { host, port, database, user, password } = config.database;

  if (!host || !database || !user || !password) {
    throw new Error(
      'Faltan variables de conexión PostgreSQL. Completa PG_HOST, PG_PORT, PG_DB, PG_USER y PG_PASS en backend/.env.',
    );
  }

  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('PG_PORT debe ser un entero entre 1 y 65535.');
  }
}

export const pool = new Pool({
  host: config.database.host,
  port: config.database.port,
  database: config.database.database,
  user: config.database.user,
  password: config.database.password,
});

export async function connectDatabase(): Promise<void> {
  validateDatabaseConfig();
  const client = await pool.connect();

  try {
    await client.query('SELECT 1');
    console.log(
      `Conexión PostgreSQL establecida en ${config.database.host}:${config.database.port}/${config.database.database}`,
    );
  } finally {
    client.release();
  }
}
