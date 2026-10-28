import assert from 'node:assert/strict';
import { test } from 'node:test';
process.env.API_KEY = 'test-only-key';
const { createApp } = await import('../dist/app.js');
const { pool } = await import('../dist/db.js');
pool.query = async () => ({ rows: [] });
test('API key and CORS preflight', async () => {
  const server = createApp('http://localhost:5173').listen(0, '127.0.0.1');
  await new Promise(resolve => server.once('listening', resolve));
  const url = `http://127.0.0.1:${server.address().port}/api/lecturas/ultima`;
  try {
    assert.equal((await fetch(url)).status, 401);
    assert.equal((await fetch(url, {headers:{'X-API-Key':'wrong'}})).status, 401);
    assert.equal((await fetch(url, {headers:{'X-API-Key':'test-only-key'}})).status, 200);
    const options = await fetch(url, {method:'OPTIONS', headers:{Origin:'http://localhost:5173','Access-Control-Request-Method':'GET','Access-Control-Request-Headers':'x-api-key'}});
    assert.equal(options.status, 204);
    assert.match(options.headers.get('access-control-allow-headers'), /X-API-Key/i);
  } finally { await new Promise(resolve => server.close(resolve)); await pool.end(); }
});