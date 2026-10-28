import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomUUID } from 'node:crypto';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { redactLogText } from '../src/mqtt/messageLogger.js';

export const DURATION_MS = 300000;
export function expectedReadings(interval: number) {
  if (!Number.isInteger(interval) || interval < 10 || interval > 30)
    throw new Error('Intervalo final requerido: entero entre 10 y 30 segundos.');
  const expected = Math.floor(DURATION_MS / 1000 / interval);
  return { expected, minimum: Math.ceil(expected * 0.98) };
}
export function percentage(saved: number, expected: number) { return saved * 100 / expected; }

interface Run {
  issue: number; runId: string; source: string; started: string; plannedEnd: string;
  baselineId: string; interval: number; database: string; host: string; port: number;
  checkpoint: string; completed: boolean; passed: boolean; incidents: { timestamp: string; message: string }[];
  counts: { variable: string; saved: number; expected: number; percentage: number; passed: boolean }[];
}

export async function monitorContinuity() {
  const mode = process.argv[2];
  if (!['start', 'resume'].includes(mode)) throw new Error('Uso: continuityRun.ts start|resume ruta.json [intervalo]');
  if (!process.argv[3]) throw new Error('Indica una ruta JSON nueva o el checkpoint que quieres reanudar.');
  const output = resolve(process.argv[3]);
  const db = new Pool({ ...config.database, connectionTimeoutMillis: 5000, query_timeout: 5000 });
  db.on('error', () => undefined); // Los fallos se registran también en cada consulta.
  let run: Run;
  let stopping = false;
  const stop = () => { stopping = true; };
  try {
    if (mode === 'start') {
      if (process.env.CONTINUITY_AULA_CONFIRMED !== 'yes' || process.env.SAMPLING_INTERVAL_FINAL_CONFIRMED !== 'yes')
        throw new Error('Confirma aula real e intervalo final de #144; consulta la documentación.');
      try { await readFile(output); throw new Error('El archivo ya existe: utiliza resume o una ruta nueva.'); }
      catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error; }
      const interval = Number(process.argv[4]); expectedReadings(interval);
      const recent = (await db.query(`SELECT tipo_variable, max(fecha_hora) AS latest FROM lecturas GROUP BY tipo_variable`)).rows;
      if (recent.length !== 3 || recent.some(row => Date.now() - new Date(row.latest).getTime() > 60000))
        throw new Error('Se necesitan las tres variables físicas con datos de menos de 60 segundos.');
      const started = Date.now();
      run = { issue: 150, runId: randomUUID(), source: 'Aula real; confirmación del operador',
        started: new Date(started).toISOString(), plannedEnd: new Date(started + DURATION_MS).toISOString(),
        baselineId: (await db.query('SELECT coalesce(max(id),0)::text AS id FROM lecturas')).rows[0].id,
        interval, database: config.database.database, host: config.database.host, port: config.database.port,
        checkpoint: '', completed: false, passed: false, incidents: [], counts: [] };
    } else {
      run = JSON.parse(await readFile(output, 'utf8')) as Run;
      expectedReadings(run.interval);
      if (run.issue !== 150 || !run.runId || !/^\d+$/.test(run.baselineId) ||
          !Number.isFinite(Date.parse(run.started)) || Date.parse(run.plannedEnd) - Date.parse(run.started) !== DURATION_MS ||
          run.database !== config.database.database || run.host !== config.database.host || run.port !== config.database.port)
        throw new Error('Checkpoint incompatible con la base o la ventana de 5 minutos.');
      if (run.completed) throw new Error('La ejecución ya está completada.');
      run.incidents.push({ timestamp: new Date().toISOString(), message: 'Monitor reanudado; revisar intervalo sin observación.' });
    }
    await mkdir(dirname(output), { recursive: true });
    process.on('SIGINT', stop); process.on('SIGTERM', stop);
    const persist = async () => { await writeFile(output, JSON.stringify(run, null, 2) + '\n'); };
    await persist();
    console.log(`Ventana física: ${run.started} → ${run.plannedEnd}. Checkpoint: ${output}`);
    do {
      try {
        const rows = (await db.query(`SELECT tipo_variable, count(*)::integer AS saved FROM lecturas
          WHERE id > $1 AND fecha_hora >= $2 AND fecha_hora < $3 GROUP BY tipo_variable`,
        [run.baselineId, run.started, run.plannedEnd])).rows;
        const { expected } = expectedReadings(run.interval);
        run.counts = ['temperatura', 'humedad', 'co2'].map(variable => {
          const saved = rows.find(row => row.tipo_variable === variable)?.saved ?? 0;
          return { variable, saved, expected, percentage: percentage(saved, expected),
            passed: saved >= Math.ceil(expected * 0.98) && saved <= expected };
        });
        run.completed = Date.now() >= Date.parse(run.plannedEnd);
        run.passed = run.completed && run.counts.every(row => row.passed);
      } catch (error) {
        run.incidents.push({ timestamp: new Date().toISOString(), message: redactLogText(String(error)) });
      }
      run.checkpoint = new Date().toISOString(); await persist();
      if (run.completed || stopping) break;
      // Revisar parada cada segundo, sin ocultar las interrupciones de servicios.
      for (let i = 0; i < 60 && !stopping && Date.now() < Date.parse(run.plannedEnd); i++)
        await new Promise(accept => setTimeout(accept, 1000));
    } while (!stopping);
    if (run.completed) {
      const client = await db.connect();
      try {
        await client.query('BEGIN');
        await client.query('SELECT pg_advisory_xact_lock(hashtext($1))', [run.runId]);
        for (const row of run.counts) {
          const description = `#150 ${run.runId} ${row.variable}`;
          await client.query(`INSERT INTO pruebas_transmision
            (descripcion, duracion_min, intervalo_s, enviados, recibidos, guardados, observaciones)
             SELECT $1,5,$2,$3,$4,$4,$5 WHERE NOT EXISTS
            (SELECT 1 FROM pruebas_transmision WHERE descripcion=$1)`,
          [description, run.interval, row.expected, row.saved, JSON.stringify({
            inicio: run.started, fin: run.plannedEnd, porcentaje: row.percentage,
            enviados: 'Esperadas teóricas, no envíos observados',
            recibidos: 'Mínimo confirmado por persistencia; no conteo MQTT independiente',
            incidencias: run.incidents, aprobado: row.passed,
          })]);
        }
        await client.query('COMMIT');
      } catch (error) { await client.query('ROLLBACK'); throw error; }
      finally { client.release(); }
    }
    console.log(JSON.stringify(run, null, 2));
    if (!run.passed) process.exitCode = 1;
  } finally {
    process.off('SIGINT', stop); process.off('SIGTERM', stop); await db.end();
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  monitorContinuity().catch(error => { console.error(redactLogText(String(error))); process.exitCode = 1; });
