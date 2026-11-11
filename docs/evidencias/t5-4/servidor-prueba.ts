import { readFile } from 'node:fs/promises';
import { createApp } from './src/app.js';
import { pool } from './src/db.js';

// Backend del repositorio sin sustituir servicios, consultas ni respuestas HTTP.
// Una conexión mantiene el esquema de prueba dentro de una transacción reversible.
pool.options.max = 1;
const client = await pool.connect();
await client.query('BEGIN');
await client.query(`CREATE SCHEMA qa_cards_${process.pid}`);
await client.query(`SET LOCAL search_path TO qa_cards_${process.pid}`);
await client.query(await readFile(new URL('../database/esquema.sql', import.meta.url), 'utf8'));
client.release();
let phase = 0;
async function insertBatch() {
  const values = phase++ % 2 === 0 ? [24.5, 48.2, 620] : [25.1, 49.3, 645];
  const timestamp = new Date().toISOString();
  for (const [index, variable] of ['temperatura', 'humedad', 'co2'].entries()) {
    await pool.query('INSERT INTO lecturas (fecha_hora, tipo_variable, valor) VALUES ($1, $2, $3)', [timestamp, variable, values[index]]);
  }
}
await insertBatch();
const server = createApp('http://127.0.0.1:5176').listen(4400, '127.0.0.1', () => console.log('Backend real de #76 + PostgreSQL de prueba en 127.0.0.1:4400'));
const timer = setInterval(() => { void insertBatch(); }, 15_000);
let stopping = false;
async function stop() {
  if (stopping) return;
  stopping = true;
  clearInterval(timer);
  await new Promise<void>(resolve => server.close(() => resolve()));
  await pool.query('ROLLBACK');
  await pool.end();
  console.log('ROLLBACK completado: esquema y lecturas temporales descartados.');
}
process.on('SIGINT', () => { void stop(); });
process.on('SIGTERM', () => { void stop(); });
