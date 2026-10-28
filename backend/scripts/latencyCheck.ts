import { createInterface } from 'node:readline/promises';
import { stdin, stdout } from 'node:process';
import { writeFile, mkdir } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import mqtt from 'mqtt';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { redactLogText } from '../src/mqtt/messageLogger.js';

export function averageLatency(values: number[]) {
  if (values.length !== 10 || values.some(value => !Number.isFinite(value) || value < 0))
    throw new Error('Se necesitan exactamente diez latencias válidas.');
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

export async function measureLatency() {
  if (!stdin.isTTY) throw new Error('Requiere observación humana del dashboard en terminal interactiva.');
  if (process.env.LATENCY_TEST_DATABASE !== config.database.database || process.env.LATENCY_SYNTHETIC_ISOLATED !== 'yes')
    throw new Error('Configura una base, broker y backend exclusivos; confirma LATENCY_SYNTHETIC_ISOLATED=yes.');
  const key = process.env.API_KEY;
  const password = process.env.MQTT_PUBLISH_PASS;
  const base = new URL(process.env.LATENCY_API_URL ?? 'http://localhost:3000/api/');
  if (!['http:', 'https:'].includes(base.protocol) || base.username || base.password || base.search || base.hash)
    throw new Error('URL de API inválida.');
  if (!key || !password || !process.env.MQTT_URL) throw new Error('Faltan API_KEY, MQTT_PUBLISH_PASS o MQTT_URL.');
  const endpoint = new URL('lecturas/ultima', base.href.endsWith('/') ? base : new URL(base.href + '/'));
  const request = (authorized: boolean) => fetch(endpoint, {
    headers: authorized ? { 'X-API-Key': key } : {}, signal: AbortSignal.timeout(5000),
  });
  if ((await request(false)).status !== 401) throw new Error('La API no exige clave: integrar #157 antes del ensayo.');
  if (!(await request(true)).ok) throw new Error('La clave no permite consultar la API.');
  const output = resolve(process.argv[2] ?? `logs/latency151-${Date.now()}.json`);
  const db = new Pool({ ...config.database, connectionTimeoutMillis: 3000, query_timeout: 3000 });
  const terminal = createInterface({ input: stdin, output: stdout });
  const result = { issue: 151, source: 'Publicaciones sintéticas; cronómetro con confirmación visual humana',
    started: new Date().toISOString(), ended: '', measurements: [] as {
      value: number; published: string; displayed: string; seconds: number; readingId: string;
    }[], averageSeconds: null as number | null, passed: false, cleaned: false, errors: [] as string[] };
  let client: mqtt.MqttClient | undefined;
  let baseline: string | undefined;
  try {
    baseline = (await db.query('SELECT coalesce(max(id),0)::text AS id FROM lecturas')).rows[0].id;
    console.log('Comprueba X-API-Key en Network y refresco normal de 15 s. No recargues la página durante cada medida.');
    if (await terminal.question('Escribe CLAVE ACTIVA para confirmar el dashboard autenticado: ') !== 'CLAVE ACTIVA')
      throw new Error('Dashboard autenticado no confirmado.');
    client = await mqtt.connectAsync(process.env.MQTT_URL, {
      username: process.env.MQTT_PUBLISH_USER ?? 'esp32', password, clean: true, reconnectPeriod: 0, connectTimeout: 5000,
    });
    for (let index = 0; index < 10; index++) {
      const value = 901 + index; // Valores deliberadamente irreales; solo en entorno aislado.
      await terminal.question(`Medida ${index + 1}: Enter cuando estés listo para publicar ${value} °C. `);
      const started = Date.now();
      await client.publishAsync('ambiente/temperatura', String(value), { qos: 0, retain: false });
      const answer = await terminal.question(`Cuando VEAS ${value} en la tarjeta, escribe ese número y Enter: `);
      const displayed = Date.now();
      if (answer.trim() !== String(value)) throw new Error('No se confirmó visualmente el valor publicado.');
      const response = await request(true);
      if (!response.ok) throw new Error('Falló la API autenticada durante la medición.');
      const readings = await response.json() as { tipo_variable: string; valor: number }[];
      if (!readings.some(row => row.tipo_variable === 'temperatura' && row.valor === value))
        throw new Error('El valor confirmado no coincide con la API actual.');
      const row = (await db.query(`SELECT id::text FROM lecturas WHERE id > $1
        AND tipo_variable='temperatura' AND valor=$2 AND fecha_hora >= $3 ORDER BY id LIMIT 1`,
      [baseline, value, new Date(started)])).rows[0];
      if (!row) throw new Error('La API y la base del ensayo no corresponden o falta persistencia.');
      result.measurements.push({ value, published: new Date(started).toISOString(), displayed: new Date(displayed).toISOString(),
        seconds: (displayed - started) / 1000, readingId: row.id });
    }
    result.averageSeconds = averageLatency(result.measurements.map(row => row.seconds));
    result.passed = result.averageSeconds < 20;
  } catch (error) { result.errors.push(redactLogText(String(error))); }
  finally {
    terminal.close();
    if (client) await client.endAsync(true);
    try {
      if (baseline !== undefined) {
        await db.query(`DELETE FROM lecturas WHERE id > $1 AND tipo_variable='temperatura' AND valor BETWEEN 901 AND 910`, [baseline]);
        result.cleaned = Number((await db.query(`SELECT count(*) FROM lecturas WHERE id > $1
          AND tipo_variable='temperatura' AND valor BETWEEN 901 AND 910`, [baseline])).rows[0].count) === 0;
      }
    } catch (error) { result.errors.push(redactLogText(String(error))); }
    await db.end(); result.ended = new Date().toISOString();
    result.passed &&= result.cleaned && result.errors.length === 0;
    await mkdir(dirname(output), { recursive: true });
    await writeFile(output, JSON.stringify(result, null, 2) + '\n');
    console.log(JSON.stringify(result, null, 2));
    if (!result.passed) process.exitCode = 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  measureLatency().catch(error => { console.error(redactLogText(String(error))); process.exitCode = 1; });
