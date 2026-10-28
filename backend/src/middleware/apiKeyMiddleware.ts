import { timingSafeEqual } from 'node:crypto';
import type { RequestHandler } from 'express';

export function apiKeyMiddleware(apiKey: string): RequestHandler {
  const expected = Buffer.from(apiKey, 'utf8');
  if (expected.length === 0) throw new Error('API_KEY debe estar definida.');
  return (request, response, next) => {
    const provided = Buffer.from(request.get('X-API-Key') ?? '', 'utf8');
    if (provided.length !== expected.length || !timingSafeEqual(provided, expected)) {
      response.status(401).json({ mensaje: 'No autorizado.' });
      return;
    }
    next();
  };
}