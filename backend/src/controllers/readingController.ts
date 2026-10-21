import type { NextFunction, Request, Response } from 'express';
import { getLatestReadings } from '../services/readingService.js';

export async function getLatestReadingsHandler(request: Request, response: Response, next: NextFunction) {
  if (/\?.+/.test(request.originalUrl)) {
    response.status(400).json({ mensaje: 'Esta ruta no admite parámetros de consulta.' });
    return;
  }

  if (request.headers['transfer-encoding'] !== undefined
    || Number(request.headers['content-length'] ?? 0) > 0) {
    response.status(400).json({ mensaje: 'Esta ruta no admite cuerpo en la solicitud.' });
    return;
  }

  try {
    response.json(await getLatestReadings());
  } catch (error) {
    next(error);
  }
}
