import { spawn, execFile, type ChildProcess } from 'node:child_process';
import { promisify } from 'node:util';
import { mkdtemp, readFile, writeFile, mkdir, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomBytes } from 'node:crypto';
import { createConnection } from 'node:net';
import { setTimeout as delay } from 'node:timers/promises';
import mqtt, { type MqttClient } from 'mqtt';
import { Pool } from 'pg';
import { config } from '../src/config.js';
import { redactLogText } from '../src/mqtt/messageLogger.js';

// Ensayo sintético: broker autenticado exclusivo y esquema desechable.
// No carga firmware, no publica al broker habitual y no modifica su configuración.
const root = fileURLToPath(new URL('../../', import.meta.url));
const output = resolve(process.env.QOS_OUTPUT_DIR ?? join(root, 'docs/evidencias/t9-2'));
const brokerPort = Number(process.env.QOS_BROKER_PORT ?? 18848);
const intervalMs = Number(process.env.QOS_INTERVAL_MS ?? 20);
const schema = `qos148_${randomBytes(6).toString('hex')}`;
const topic = 'ambiente/temperatura';
const execute = promisify(execFile);
const executable = (name: string) => process.platform === 'win32'
  ? `C:\\Program Files\\mosquitto\\${name}.exe` : name;

interface Entry { timestamp: string; event: string; topic: string; content: string }
interface Round {
  publicationQos: 0 | 1; subscriptionRequested: number; subscriptionGranted: number;
  effectiveQos: number; deliveredQos: number[]; payloadFrom: number; payloadTo: number;
  started: string; ended?: string; attempted: number; sent: number;
  received: number; uniqueReceived: number; duplicates: number;
  savedBefore: number; savedAfter: number; savedUnique: number;
  transportLossPercent?: number; persistenceLossPercent?: number;
}
const entries: Entry[] = [];
const databaseErrors: string[] = [];
const rounds: Round[] = [];
const pending = new Set<Promise<void>>();
const result = {
  issue: 148, started: new Date().toISOString(), source: 'Datos sintéticos; no ESP32',
  environment: { node: process.version, postgres: '', mosquitto: '', backendCommit: process.env.QOS_SOURCE_COMMIT ?? '' },
  isolation: { schema, broker: `mqtt://127.0.0.1:${brokerPort}`, authentication: true,
    acl: 'infra/mosquitto/acl', publicBefore: [] as unknown[], publicAfter: [] as unknown[],
    schemaDropped: false, brokerStopped: false, temporaryFilesRemoved: false },
  publication: { topic, perLevel: 500, intervalMs, retain: false,
    sentDefinition: 'Callback de publish sin error; QoS 0 no confirma recepción del broker. QoS 1 espera PUBACK.' },
  rounds, databaseErrors, errors: [] as string[], passed: false, ended: '',
};

async function bounded<T>(operation: Promise<T>, ms = 15000): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    return await Promise.race([operation, new Promise<never>((_, reject) => {
      timer = setTimeout(() => reject(new Error('Se agotó el tiempo de espera del ensayo.')), ms);
    })]);
  } finally { clearTimeout(timer); }
}

function portOpen(): Promise<boolean> {
  return new Promise(accept => {
    const socket = createConnection({ host: '127.0.0.1', port: brokerPort });
    socket.setTimeout(500);
    const finish = (open: boolean) => { socket.destroy(); accept(open); };
    socket.once('connect', () => finish(true));
    socket.once('error', () => finish(false));
    socket.once('timeout', () => finish(false));
  });
}

async function main() {
  if (!Number.isInteger(brokerPort) || brokerPort < 1024 || brokerPort > 65535 || !Number.isFinite(intervalMs) || intervalMs < 0)
    throw new Error('Puerto o intervalo inválidos.');
  // Requiere elegir explícitamente la BD de ensayo, nunca toma la habitual por omisión.
  if (process.env.QOS_TEST_DATABASE !== config.database.database)
    throw new Error('Define PG_* para la BD de ensayo y QOS_TEST_DATABASE con el mismo nombre.');
  const admin = new Pool({ ...config.database, options: '-c search_path=public' });
  let backendPool: Pool | undefined;
  let subscriber: MqttClient | undefined;
  let publisher: MqttClient | undefined;
  let broker: ChildProcess | undefined;
  let directory: string | undefined;
  let created = false;
  const previousOptions = process.env.PGOPTIONS;
  try {
    result.environment.postgres = (await admin.query('SHOW server_version')).rows[0].server_version;
    const snapshot = async () => (await admin.query(`SELECT table_name FROM information_schema.tables
      WHERE table_schema='public' AND table_type='BASE TABLE' ORDER BY table_name`)).rows;
    // El ensayo solo escribe en el esquema creado; conserva inventario/conteos de public.
    const publicSnapshot = async () => {
      const tables = await snapshot();
      return await Promise.all(tables.map(async ({ table_name }: { table_name: string }) => {
        const name = '"' + table_name.replaceAll('"', '""') + '"';
        return { table: table_name, count: (await admin.query(`SELECT count(*) FROM public.${name}`)).rows[0].count };
      }));
    };
    result.isolation.publicBefore = await publicSnapshot();
    await admin.query(`CREATE SCHEMA ${schema}`); created = true;
    process.env.PGOPTIONS = `-c search_path=${schema}`;
    const db = await import('../src/db.js');
    backendPool = db.pool;
    await backendPool.query(await readFile(join(root, 'database/esquema.sql'), 'utf8'));
    directory = await mkdtemp(join(process.env.TEMP ?? tmpdir(), 'qos148-'));
    const passwordFile = join(directory, 'passwd');
    const backendPassword = randomBytes(24).toString('hex');
    const publisherPassword = randomBytes(24).toString('hex');
    process.env.MQTT_PASS = backendPassword;
    process.env.MQTT_PUBLISH_PASS = publisherPassword;
    await execute(process.env.MOSQUITTO_PASSWD_PATH ?? executable('mosquitto_passwd'), ['-b', '-c', passwordFile, 'backend', backendPassword]);
    await execute(process.env.MOSQUITTO_PASSWD_PATH ?? executable('mosquitto_passwd'), ['-b', passwordFile, 'esp32', publisherPassword]);
    const brokerConfig = join(directory, 'mosquitto.conf');
    await writeFile(brokerConfig, `listener ${brokerPort} 127.0.0.1\nallow_anonymous false\npassword_file ${passwordFile.replaceAll('\\', '/')}\nacl_file ${join(root, 'infra/mosquitto/acl').replaceAll('\\', '/')}\npersistence false\nlog_dest stdout\n`);
    if (await portOpen()) throw new Error('El puerto de ensayo ya está ocupado; elige QOS_BROKER_PORT.');
    const brokerExecutable = process.env.MOSQUITTO_PATH ?? executable('mosquitto');
    const help = await execute(brokerExecutable, ['-h']);
    result.environment.mosquitto = (help.stdout + help.stderr).match(/mosquitto version ([\d.]+)/)?.[1] ?? 'Consultar instalación';
    broker = spawn(brokerExecutable, ['-c', brokerConfig, '-v'], { stdio: ['ignore', 'pipe', 'pipe'] });
    let brokerLog = '';
    let spawnError: Error | undefined;
    broker.on('error', error => { spawnError = error; });
    const log = (data: Buffer) => { brokerLog += data.toString(); };
    broker.stdout!.on('data', log); broker.stderr!.on('data', log);
    const startDeadline = Date.now() + 10000;
    while (!(await portOpen())) {
      if (spawnError) throw spawnError;
      if (broker.exitCode !== null || Date.now() > startDeadline)
        throw new Error(`No arrancó el broker de ensayo: ${brokerLog}`);
      await delay(100);
    }
    process.env.MQTT_URL = result.isolation.broker;
    process.env.MQTT_USER = 'backend';
    const { startMqttSubscriber } = await import('../src/mqtt/mqttSubscriber.js');
    let requested = -1, granted = -1;
    const delivered = new Set<number>();
    subscriber = startMqttSubscriber({ insertReading(reading) {
      const job = db.insertReading(reading).catch(error => {
        databaseErrors.push(redactLogText(String(error))); throw error;
      });
      pending.add(job);
      void job.finally(() => pending.delete(job)).catch(() => undefined);
      return job;
    } }, (event, receivedTopic, content) => {
      entries.push({ timestamp: new Date().toISOString(), event, topic: receivedTopic, content });
    });
    await bounded(new Promise<void>((accept, reject) => {
      subscriber!.on('error', reject);
      subscriber!.on('packetsend', packet => {
        if (packet.cmd === 'subscribe') requested = packet.subscriptions[0].qos;
      });
      subscriber!.on('packetreceive', packet => {
        if (packet.cmd === 'suback') {
          const value = packet.granted[0];
          if (typeof value !== 'number') { reject(new Error('SUBACK inválido.')); return; }
          granted = value; accept();
        }
        if (packet.cmd === 'publish' && packet.topic === topic) delivered.add(packet.qos);
      });
    }));
    if (granted !== 0 || requested !== 0) throw new Error('Cambió la suscripción de producción; revisar el ensayo.');
    publisher = await bounded(mqtt.connectAsync(result.isolation.broker, {
      username: 'esp32', password: publisherPassword, clean: true, reconnectPeriod: 0,
    }));
    for (const qos of [0, 1] as const) {
      const base = 10000 + qos * 1000;
      const count = async () => (await backendPool!.query(`SELECT count(*) AS total, count(DISTINCT valor) AS unique
        FROM lecturas WHERE tipo_variable=$1 AND valor BETWEEN $2 AND $3`, ['temperatura', base, base + 499])).rows[0];
      const round: Round = { publicationQos: qos, subscriptionRequested: requested, subscriptionGranted: granted,
        effectiveQos: Math.min(qos, granted), deliveredQos: [], payloadFrom: base, payloadTo: base + 499,
        started: new Date().toISOString(), attempted: 0, sent: 0, received: 0, uniqueReceived: 0, duplicates: 0,
        savedBefore: Number((await count()).total), savedAfter: 0, savedUnique: 0 };
      rounds.push(round); delivered.clear();
      try {
        for (let i = 0; i < 500; i++) {
          round.attempted++;
          await bounded(publisher.publishAsync(topic, String(base + i), { qos, retain: false }));
          round.sent++;
          await delay(intervalMs);
        }
      } finally {
        const deadline = Date.now() + 15000;
        do {
          await delay(250);
          const matched = entries.filter(e => e.event === 'received' && e.topic === topic && Number(e.content) >= base && Number(e.content) <= base + 499);
          round.received = matched.length;
          round.uniqueReceived = new Set(matched.map(e => e.content)).size;
          round.duplicates = round.received - round.uniqueReceived;
          const saved = await count();
          round.savedAfter = Number(saved.total); round.savedUnique = Number(saved.unique);
          if (round.uniqueReceived >= round.sent && round.savedUnique >= round.sent && pending.size === 0) break;
        } while (Date.now() < deadline);
        round.deliveredQos = [...delivered]; round.ended = new Date().toISOString();
        round.transportLossPercent = round.sent ? 100 * (round.sent - round.uniqueReceived) / round.sent : undefined;
        round.persistenceLossPercent = round.sent ? 100 * (round.sent - round.savedUnique) / round.sent : undefined;
      }
    }
    result.isolation.publicAfter = await publicSnapshot();
    if (JSON.stringify(result.isolation.publicBefore) !== JSON.stringify(result.isolation.publicAfter))
      throw new Error('Cambió el inventario/conteo público durante la prueba.');
  } catch (error) {
    result.errors.push(redactLogText(error instanceof Error ? error.message : String(error)));
  } finally {
    for (const client of [publisher, subscriber]) {
      if (client) await client.endAsync(true).catch(error => result.errors.push(redactLogText(String(error))));
    }
    await Promise.allSettled([...pending]);
    if (backendPool) await backendPool.end();
    if (created) {
      try {
        await admin.query(`DROP SCHEMA ${schema} CASCADE`);
        result.isolation.schemaDropped = !(await admin.query('SELECT 1 FROM pg_namespace WHERE nspname=$1', [schema])).rowCount;
      } catch (error) { result.errors.push(redactLogText(String(error))); }
    }
    await admin.end();
    if (previousOptions === undefined) delete process.env.PGOPTIONS; else process.env.PGOPTIONS = previousOptions;
    if (broker && broker.exitCode === null && broker.signalCode === null) {
      await bounded(new Promise<void>(accept => { broker!.once('exit', () => accept()); broker!.kill(); })).catch(error => result.errors.push(String(error)));
    }
    result.isolation.brokerStopped = broker !== undefined && (broker.exitCode !== null || broker.signalCode !== null);
    if (directory) {
      await rm(directory, { recursive: true, force: true }); result.isolation.temporaryFilesRemoved = true;
    }
    result.passed = result.errors.length === 0 && databaseErrors.length === 0 && rounds.length === 2 &&
      rounds.every(r => r.sent === 500 && r.uniqueReceived === 500 && r.savedUnique === 500) &&
      result.isolation.schemaDropped && result.isolation.brokerStopped && result.isolation.temporaryFilesRemoved;
    result.ended = new Date().toISOString();
    await mkdir(output, { recursive: true });
    await writeFile(join(output, 'resultados.json'), JSON.stringify(result, null, 2) + '\n');
    await writeFile(join(output, 'recepciones-backend.jsonl'), entries.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(JSON.stringify(result, null, 2));
    if (!result.passed) process.exitCode = 1;
  }
}

main().catch(error => { console.error(redactLogText(String(error))); process.exitCode = 1; });
