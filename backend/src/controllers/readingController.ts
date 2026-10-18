import type { Request, Response } from 'express';
import { getLatestReadings } from '../services/readingService.js';

export async function getLatestReadingsHandler(request: Request, response: Response) {
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
  } catch {
    response.status(500).json({ mensaje: 'No se pudo completar la solicitud.' });
  }
}
