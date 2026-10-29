import { createInterface } from 'node:readline/promises';
import { stdin, stdout } from 'node:process';
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { redactLogText } from '../src/mqtt/messageLogger.js';

export function recoverySeconds(start: number, observed: number): number {
  if (!Number.isFinite(start) || !Number.isFinite(observed) || observed < start)
    throw new Error('Tiempos inválidos.');
  return (observed - start) / 1000;
}

export async function runRecovery() {
  const scenario = process.argv[2];
  if (!['wifi', 'broker', 'backend'].includes(scenario)) throw new Error('Indica wifi, broker o backend.');
  const output = resolve(process.argv[3] ?? `logs/recovery149-${scenario}-${Date.now()}.json`);
  if (!stdin.isTTY) throw new Error('Requiere terminal interactiva y observación física de la falla.');
  const db = new Pool({ ...config.database, connectionTimeoutMillis: 2000, query_timeout: 2000 });
  const terminal = createInterface({ input: stdin, output: stdout });
  const result = { issue: 149, scenario, started: '', ended: '', baselineId: '',
    seconds: null as number | null, recovered: null as unknown,
    source: 'ESP32 físico; confirmación de interrupción por operador',
    passed: false, errors: [] as string[] };
  db.on('error', error => { result.errors.push(redactLogText(String(error))); });
  try {
    const latest = (await db.query(`SELECT id::text, fecha_hora FROM lecturas ORDER BY lecturas.id DESC LIMIT 1`)).rows[0];
    if (!latest || Date.now() - new Date(latest.fecha_hora).getTime() > 60000)
      throw new Error('Primero confirma lecturas físicas recientes, de menos de 60 segundos.');
    result.baselineId = latest.id;
    console.log(`Caso ${scenario}. El monitor NO provoca la falla ni reinicia servicios.`);
    console.log('Interrumpe solo ese componente. Tras confirmar la interrupción, escribe FALLO CONFIRMADO.');
    if (await terminal.question('Confirmación: ') !== 'FALLO CONFIRMADO') throw new Error('Interrupción no confirmada.');
    const started = Date.now();
    result.started = new Date(started).toISOString();
    // El límite de id se toma ANTES de interrumpir; fecha_hora excluye el flujo previo.
    console.log('Cronómetro iniciado. Restablece WiFi o inicia el componente según el procedimiento; no intervengas en la reconexión.');
    while (Date.now() - started < 120000) {
      try {
        const row = (await db.query(`SELECT id::text, tipo_variable, valor, fecha_hora FROM lecturas
          WHERE id > $1 AND fecha_hora >= $2 ORDER BY fecha_hora, id LIMIT 1`,
        [result.baselineId, result.started])).rows[0];
        if (row) {
          result.ended = new Date().toISOString();
          result.seconds = recoverySeconds(started, Date.parse(result.ended));
          result.recovered = row; result.passed = result.seconds < 60;
          break;
        }
      } catch (error) { result.errors.push(redactLogText(String(error))); }
      await delay(250);
    }
    if (result.seconds === null) result.errors.push('Sin fila nueva durante 120 s.');
  } catch (error) {
    result.errors.push(redactLogText(String(error)));
  } finally {
    result.ended ||= new Date().toISOString();
    terminal.close(); await db.end();
    await mkdir(dirname(output), { recursive: true });
    await writeFile(output, JSON.stringify(result, null, 2) + '\n');
    console.log(JSON.stringify(result, null, 2));
    if (!result.passed) process.exitCode = 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  runRecovery().catch(error => { console.error(redactLogText(String(error))); process.exitCode = 1; });
}
