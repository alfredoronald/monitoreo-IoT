import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { readFileSync, existsSync } from 'node:fs';
import { randomInt } from 'node:crypto';
import { setTimeout as delay } from 'node:timers/promises';
import { connectDatabase, pool } from '../src/db.js';
import { defaultMessageLogPath, redactLogText } from '../src/mqtt/messageLogger.js';

const execute = promisify(execFile);
const topic = 'ambiente/co2';
const count = 100;
const qos = 0;

async function main() {
  const password = process.env.MQTT_PUBLISH_PASS;
  if (!password) throw new Error('Define MQTT_PUBLISH_PASS con la contraseña local de esp32. Consulta el README.');
  const broker = new URL(process.env.MQTT_URL ?? 'mqtt://localhost:1883');
  if (broker.protocol !== 'mqtt:') throw new Error('Esta prueba requiere MQTT sin TLS (mqtt://).');
  const executable = process.env.MOSQUITTO_PUB_PATH ?? (process.platform === 'win32'
    ? 'C:\\Program Files\\mosquitto\\mosquitto_pub.exe' : 'mosquitto_pub');
  await connectDatabase();
  if (!existsSync(defaultMessageLogPath)) throw new Error('Inicia el backend y envía un mensaje primero para crear su log.');

  // Un rango nuevo permite separar la prueba de lecturas anteriores sin borrarlas.
  let base: number;
  do {
    base = randomInt(5000, 9000) * 100;
  } while (Number((await pool.query('SELECT count(*) FROM lecturas WHERE tipo_variable = $1 AND valor BETWEEN $2 AND $3',
    ['co2', base, base + count - 1])).rows[0].count) > 0);
  const offset = readFileSync(defaultMessageLogPath).length;
  const baseline = (await pool.query('SELECT COALESCE(MAX(id), 0) AS id FROM lecturas')).rows[0].id;
  let sent = 0;
  console.log(`Prueba #22: ${topic}, QoS publicación/suscripción: ${qos}, valores ${base}–${base + count - 1}.`);
  console.log('Son valores sintéticos de prueba; no representan mediciones del aula.');
  for (let index = 0; index < count; index++) {
    try {
      await execute(executable, ['-h', broker.hostname, '-p', broker.port || '1883',
        '-u', 'esp32', '-P', password, '-q', String(qos), '-t', topic, '-m', String(base + index)],
      { timeout: 10000 });
      sent++;
    } catch {
      throw new Error(`Falló mosquitto_pub después de ${sent} envíos; revisa broker, ruta y credenciales. La prueba está incompleta.`);
    }
  }

  let received = 0;
  let saved = 0;
  let unique = 0;
  const deadline = Date.now() + 15000;
  do {
    await delay(500);
    const entries = readFileSync(defaultMessageLogPath).subarray(offset).toString('utf8')
      .split('\n').filter(Boolean).map((line) => JSON.parse(line));
    const matched = entries.filter((entry) => entry.event === 'received' && entry.topic === topic
      && /^\d+$/.test(entry.content) && Number(entry.content) >= base && Number(entry.content) < base + count);
    received = matched.length;
    unique = new Set(matched.map((entry) => entry.content)).size;
    saved = Number((await pool.query(
      'SELECT count(*) FROM lecturas WHERE id > $1 AND tipo_variable = $2 AND valor BETWEEN $3 AND $4',
      [baseline, 'co2', base, base + count - 1],
    )).rows[0].count);
  } while ((unique < sent || saved < sent) && Date.now() < deadline);

  console.log('\n| Enviados | Recibidos (log) | Guardados (BD) | QoS | No recibidos | No guardados |');
  console.log('|---:|---:|---:|---:|---:|---:|');
  console.log(`| ${sent} | ${received} | ${saved} | ${qos} | ${sent - unique} | ${Math.max(0, unique - saved)} |`);
  console.log(`Recibidos únicos: ${unique}. Duplicados en log: ${received - unique}.`);
  console.log(`Conteo de BD: id > ${baseline}, tipo_variable = 'co2', valor entre ${base} y ${base + count - 1}.`);
  console.log('Ventana máxima de espera tras publicar: 15 segundos. Revisa errores de BD si faltan guardados.');
  if (received !== sent || saved !== sent) {
    console.log('Hay pérdidas o duplicados: conserva estos resultados reales en la evidencia.');
    process.exitCode = 1;
  }
}

try {
  await main();
} catch (error) {
  console.error(redactLogText(error instanceof Error ? error.message : String(error)));
  process.exitCode = 1;
} finally {
  await pool.end();
}
