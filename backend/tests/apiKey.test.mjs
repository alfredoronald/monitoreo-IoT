import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
process.env.API_KEY = 'test-only-key';
const { createApp } = await import('../src/app.ts');
const { pool } = await import('../src/db.ts');
pool.query = async () => ({ rows: [] });
test('Todas las rutas requieren la clave y CORS permite preflight', async () => {
  const server = createApp('http://localhost:5173').listen(0, '127.0.0.1');
  await new Promise(resolve => server.once('listening', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  try {
    for (const path of ['/api/lecturas/ultima','/api/lecturas?variable=co2&desde=2026-10-15T12:00:00Z&hasta=2026-10-15T13:00:00Z','/api/estado-aire','/api/alertas?atendida=false','/api/alertas/7']) {
      const method = path.endsWith('/7') ? 'PATCH' : 'GET';
      assert.equal((await fetch(base+path,{method})).status, 401);
      assert.equal((await fetch(base+path,{method,headers:{'X-API-Key':'incorrecta'}})).status, 401);
      assert.notEqual((await fetch(base+path,{method,headers:{'X-API-Key':'test-only-key'}})).status, 401);
    }
    assert.equal((await fetch(base+'/api/lecturas/ultima',{headers:{'X-API-Key':'test-only-key'}})).status, 200);
    const options = await fetch(base+'/api/lecturas/ultima', {method:'OPTIONS', headers:{Origin:'http://localhost:5173','Access-Control-Request-Method':'GET','Access-Control-Request-Headers':'x-api-key'}});
    assert.equal(options.status, 204);
    assert.match(options.headers.get('access-control-allow-headers'), /X-API-Key/i);
  } finally { await new Promise(resolve => server.close(resolve)); await pool.end(); }
});
test('El servidor no arranca sin API_KEY', () => {
  const env = {...process.env, API_KEY: ''};
  const result = spawnSync(process.execPath, ['dist/index.js'], { env, cwd:process.cwd(), encoding:'utf8', timeout:5000 });
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /API_KEY/);
});
