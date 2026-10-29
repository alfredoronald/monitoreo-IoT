import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { redactLogText } from '../src/mqtt/messageLogger.js';

interface Metric {
  key: string; description: string; sent: number; received: number; saved: number;
  qos: number | null; interval: number | null; durationMinutes: number | null;
  metrics: Record<string, unknown>;
}
const count = (value: unknown) => {
  if (!Number.isSafeInteger(value) || Number(value) < 0) throw new Error('Conteo inválido.');
  return Number(value);
};
const seconds = (value: unknown) => {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < 0) throw new Error('Tiempo inválido.');
  return value;
};

// Ordena objetos, no arrays: cambia el formato del archivo, no su significado.
function canonicalValue(value: any): any {
  if (Array.isArray(value)) return value.map(canonicalValue);
  if (value !== null && typeof value === 'object')
    return Object.fromEntries(Object.keys(value).sort().map(key => [key, canonicalValue(value[key])]));
  return value;
}
export function evidenceIdentity(raw: string) {
  const data = JSON.parse(raw);
  const canonicalSha256 = createHash('sha256').update(JSON.stringify(canonicalValue(data))).digest('hex');
  return { data, canonicalSha256,
    identity: data.issue === 150 ? data.runId : `${data.started ?? ''}-v2-${canonicalSha256}` };
}

// Solo admite los JSON generados por los ensayos de #148–#151.
export function normalizeEvidence(data: any, identity: string): Metric[] {
  const make = (suffix: string, values: Omit<Metric, 'key' | 'description'>): Metric => ({
    key: `${data.issue}:${identity}:${suffix}`, description: `#${data.issue} ${identity} ${suffix}`, ...values,
  });
  if (data.issue === 148) {
    if (!Array.isArray(data.rounds) || data.rounds.length !== 2 ||
        new Set(data.rounds.map((row: any) => row.publicationQos)).size !== 2 || !data.started)
      throw new Error('Evidencia QoS incompleta.');
    return data.rounds.map((row: any) => {
      if (![0,1].includes(row.publicationQos) || ![0,1].includes(row.subscriptionGranted)) throw new Error('QoS inválido.');
      return make(`qos${row.publicationQos}`, { sent: count(row.sent), received: count(row.uniqueReceived),
        saved: count(row.savedUnique), qos: row.publicationQos, interval: null, durationMinutes: null,
        metrics: { publicacion_qos: row.publicationQos, suscripcion_qos: row.subscriptionGranted,
          qos_efectivo: Math.min(row.publicationQos, row.subscriptionGranted),
          recibidos_eventos: count(row.received), duplicados: count(row.duplicates),
          periodo_publicacion_ms: seconds(data.publication.intervalMs), inicio: row.started, fin: row.ended } });
    });
  }
  if (data.issue === 149) {
    if (!['wifi','broker','backend'].includes(data.scenario) || !data.recovered?.id)
      throw new Error('La recuperación no tiene primera fila verificada.');
    const elapsed = seconds(data.seconds);
    return [make(data.scenario, { sent: 0, received: 0, saved: 1, qos: null, interval: null,
      durationMinutes: elapsed / 60, metrics: { recuperacion_segundos: elapsed, aprobado: elapsed < 60,
        lectura_id: String(data.recovered.id), inicio: data.started, fin: data.ended,
        conteos: 'Enviados y recibidos no medidos (0); guardados=primera fila de recuperación.' } })];
  }
  if (data.issue === 150) {
    if (!data.completed || Date.parse(data.plannedEnd) - Date.parse(data.started) !== 300000 ||
        typeof data.runId !== 'string' || !data.runId ||
        !Number.isInteger(data.interval) || data.interval < 10 || data.interval > 30 || !Array.isArray(data.counts) ||
        data.counts.length !== 3 || new Set(data.counts.map((row: any) => row.variable)).size !== 3)
      throw new Error('Se requiere una ventana de 5 minutos con intervalo final.');
    return data.counts.map((row: any) => {
      if (!['temperatura','humedad','co2'].includes(row.variable)) throw new Error('Variable inválida.');
      const expected = Math.floor(300 / data.interval), saved = count(row.saved);
      return make(row.variable, { sent: expected, received: saved, saved, qos: null,
        interval: data.interval, durationMinutes: 5, metrics: { esperadas: expected, porcentaje: saved * 100 / expected,
          aprobado: saved >= Math.ceil(expected * .98) && saved <= expected,
          inicio: data.started, fin: data.plannedEnd, incidencias: data.incidents,
          conteos: 'Enviados=esperadas teóricas; recibidos=mínimo confirmado por persistencia, no medición MQTT independiente.' } });
    });
  }
  if (data.issue === 151) {
    if (!Array.isArray(data.measurements) || data.measurements.length !== 10) throw new Error('Faltan las diez latencias.');
    const values = data.measurements.map((row: any) => seconds(row.seconds));
    const average = values.reduce((sum: number, value: number) => sum + value, 0) / 10;
    return [make('latencia', { sent: 10, received: 10, saved: 10, qos: 0, interval: null, durationMinutes: null,
      metrics: { latencias_segundos: values, promedio_segundos: average, aprobado: average < 20,
        conteos: '10 publicaciones, observaciones visuales y filas confirmadas; no conteo independiente de eventos MQTT.',
        inicio: data.started, fin: data.ended, datos_sinteticos_eliminados: data.cleaned === true } })];
  }
  throw new Error('Formato desconocido; se requieren resultados de #148, #149, #150 o #151.');
}

export async function importResults() {
  const args = process.argv.slice(2);
  const apply = args.includes('--apply');
  const paths = args.filter(value => value !== '--apply');
  if (!paths.length) throw new Error('Indica uno o más archivos JSON de resultados. --apply persiste en PostgreSQL.');
  const imported: { file: string; sha256: string; canonicalSha256: string; rows: Metric[] }[] = [];
  for (const file of paths) {
    const raw = await readFile(resolve(file), 'utf8');
    const { data, canonicalSha256, identity } = evidenceIdentity(raw);
    const hash = createHash('sha256').update(raw).digest('hex');
    imported.push({ file: file.replaceAll('\\','/'), sha256: hash, canonicalSha256, rows: normalizeEvidence(data, identity) });
  }
  const rows = imported.flatMap(source => source.rows);
  if (new Set(rows.map(row => row.key)).size !== rows.length) throw new Error('Hay ejecuciones duplicadas en la entrada.');
  const persistence: { key: string; id: string; lostPercent: string | null }[] = [];
  if (apply) {
    const pool = new Pool({ ...config.database, connectionTimeoutMillis: 5000 });
    try {
      const client = await pool.connect();
      try {
        await client.query('BEGIN');
        await client.query(await readFile(new URL('../../database/001_extend_transmission_metrics.sql', import.meta.url), 'utf8'));
        for (const row of rows) {
          await client.query('SELECT pg_advisory_xact_lock(hashtext($1))', [row.key]);
          // Migra claves v1 solo si coinciden ensayo, caso y métricas completas.
          // No borra ni fusiona filas ambiguas: revierte la transacción para revisión.
          const issue = row.key.split(':')[0], suffix = row.key.split(':').at(-1);
          if (issue !== '150') {
            const legacyPrefix = row.key.slice(0, row.key.indexOf('-v2-'));
            const legacyPattern = `^${legacyPrefix.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}-[a-f0-9]{12}:${suffix}$`;
            const matches = (await client.query(`SELECT id, ejecucion_clave FROM pruebas_transmision
              WHERE ejecucion_clave=$1 OR (ejecucion_clave ~ $2 AND metricas=$3::jsonb) FOR UPDATE`,
            [row.key, legacyPattern, JSON.stringify(row.metrics)])).rows;
            if (matches.length > 1) throw new Error('Evidencia con filas duplicadas: revisar antes de migrar claves.');
            if (matches.length === 1 && matches[0].ejecucion_clave !== row.key)
              await client.query('UPDATE pruebas_transmision SET ejecucion_clave=$1, descripcion=$2 WHERE id=$3',
                [row.key, row.description, matches[0].id]);
          }
          // Reutiliza las filas insertadas por el monitor #150, si existen.
          await client.query('UPDATE pruebas_transmision SET ejecucion_clave=$1 WHERE descripcion=$2 AND ejecucion_clave IS NULL', [row.key, row.description]);
          const stored = (await client.query(`INSERT INTO pruebas_transmision
            (ejecucion_clave,descripcion,duracion_min,qos,intervalo_s,enviados,recibidos,guardados,observaciones,metricas)
            VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)
            ON CONFLICT (ejecucion_clave) DO UPDATE SET metricas=EXCLUDED.metricas,
            observaciones=EXCLUDED.observaciones
            RETURNING id::text, perdidos_pct`, [row.key, row.description, row.durationMinutes, row.qos, row.interval,
            row.sent, row.received, row.saved, JSON.stringify(row.metrics), row.metrics])).rows[0];
          persistence.push({ key: row.key, id: stored.id, lostPercent: stored.perdidos_pct });
        }
        await client.query('COMMIT');
      } catch (error) { await client.query('ROLLBACK'); throw error; }
      finally { client.release(); }
    } finally { await pool.end(); }
  }
  const directory = resolve(process.env.TRANSMISSION_OUTPUT_DIR ?? 'logs/transmission152');
  await mkdir(directory, { recursive: true });
  const present = new Set(rows.map(row => Number(row.key.split(':')[0])));
  const recovery = new Set(rows.filter(row => row.key.startsWith('149:')).map(row => row.metrics && row.key.split(':').at(-1)));
  const complete = [148,149,150,151].every(issue => present.has(issue)) && recovery.size === 3;
  const report = { issue: 152, persisted: apply, complete, pending: [148,149,150,151].filter(issue => !present.has(issue)),
    recoveryCases: [...recovery], identityVersion: 2,
    sources: imported.map(({ file, sha256, canonicalSha256 }) => ({ file, sha256, canonicalSha256 })), rows, persistence };
  await writeFile(resolve(directory, 'resumen.json'), JSON.stringify(report, null, 2) + '\n');
  const markdown = ['# Resumen de transmisión — #152', '',
    `Estado: ${complete && apply ? 'cuatro grupos registrados; revisar criterios' : 'PARCIAL: no completa #152'}.`, '',
    '| Prueba | Enviados* | Recibidos* | Guardados | Métricas |', '|---|---:|---:|---:|---|',
    ...rows.map(row => `| ${row.description} | ${row.sent} | ${row.received} | ${row.saved} | ${JSON.stringify(row.metrics).replaceAll('|','/')} |`), '',
    '*Las definiciones de los conteos dependen del ensayo y figuran en métricas. 0 puede representar no medido; no inferir pérdidas de recuperación de esos campos.',
    '', `Pendientes: ${report.pending.map(issue => '#' + issue).join(', ') || 'consultar casos y criterios'}.`,
    'La presencia de filas no equivale a aprobar las pruebas; conservar y corregir los resultados fallidos.'];
  await writeFile(resolve(directory, 'resumen.md'), markdown.join('\n') + '\n');
  console.log(JSON.stringify({ persisted: apply, complete, rows: rows.length, pending: report.pending, directory }));
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  importResults().catch(error => { console.error(redactLogText(String(error))); process.exitCode = 1; });
