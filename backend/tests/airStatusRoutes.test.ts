import assert from 'node:assert/strict';
import { request } from 'node:http';
import test from 'node:test';
import { createApp } from '../src/app.js';
import { pool } from '../src/db.js';

function httpRequest(port: number, method: string, path: string, body?: string, contentType?: string) {
  return new Promise<{ status: number; body: unknown }>((resolve, reject) => {
    const client = request({
      port,
      method,
      path,
      headers: { 'X-API-Key': 'test-only-key',
        ...(body === undefined ? {} : { 'content-length': Buffer.byteLength(body) }),
        ...(contentType === undefined ? {} : { 'content-type': contentType }),
      },
    }, (response) => {
      let text = '';
      response.setEncoding('utf8');
      response.on('data', (chunk) => { text += chunk; });
      response.on('end', () => resolve({ status: response.statusCode!, body: JSON.parse(text) }));
    });
    client.on('error', reject);
    if (body !== undefined) client.write(body);
    client.end();
  });
}

test('expone estado de aire, alertas pendientes y marca una alerta', async (context) => {
  context.mock.method(pool, 'query', async (sql: string, values: unknown[] = []) => {
    if (sql.includes('FROM v_estado_aire')) {
      return { rows: [{ tipo_variable: 'co2', valor: '900.00', fecha_hora: new Date('2026-10-15T16:00:15Z'), nivel: 'moderado', etiqueta: 'Aire moderado', recomendacion: 'Conviene ventilar pronto.' }] };
    }
    if (sql.includes('UPDATE alertas')) {
      assert.deepEqual(values, ['7']);
      return { rows: [{ id: '7', lectura_id: '102', umbral_id: '2', mensaje: 'El CO2 estimado pasó a nivel moderado.', fecha_hora: new Date('2026-10-15T16:00:15Z'), atendida: true }] };
    }
    assert.match(sql, /FROM alertas/);
    assert.deepEqual(values, [false]);
    return { rows: [{ id: '7', lectura_id: '102', umbral_id: '2', mensaje: 'El CO2 estimado pasó a nivel moderado.', fecha_hora: new Date('2026-10-15T16:00:15Z'), atendida: false }] };
  });

  const server = createApp('*', 'test-only-key').listen(0);
  await new Promise<void>((resolve) => server.once('listening', resolve));
  const port = (server.address() as { port: number }).port;

  try {
    const status = await httpRequest(port, 'GET', '/api/estado-aire');
    assert.equal(status.status, 200);
    assert.deepEqual(status.body, {
      tipo_variable: 'co2', valor: 900, fecha_hora: '2026-10-15T16:00:15.000Z',
      nivel: 'moderado', etiqueta: 'Aire moderado', recomendacion: 'Conviene ventilar pronto.',
    });

    const pending = await httpRequest(port, 'GET', '/api/alertas?atendida=false');
    assert.equal(pending.status, 200);
    assert.deepEqual(pending.body, [{
      id: '7', lectura_id: '102', umbral_id: '2', mensaje: 'El CO2 estimado pasó a nivel moderado.',
      fecha_hora: '2026-10-15T16:00:15.000Z', atendida: false,
    }]);

    const updated = await httpRequest(port, 'PATCH', '/api/alertas/7', '{"atendida":true}', 'application/json');
    assert.equal(updated.status, 200);
    assert.equal((updated.body as { atendida: boolean }).atendida, true);
  } finally {
    await new Promise<void>((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
});

test('rechaza un PATCH sin JSON y un filtro atendida inválido', async () => {
  const server = createApp('*', 'test-only-key').listen(0);
  await new Promise<void>((resolve) => server.once('listening', resolve));
  const port = (server.address() as { port: number }).port;
  try {
    assert.equal((await httpRequest(port, 'PATCH', '/api/alertas/7', '{"atendida":true}')).status, 415);
    assert.equal((await httpRequest(port, 'GET', '/api/alertas?atendida=1')).status, 400);
  } finally {
    await new Promise<void>((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
});
