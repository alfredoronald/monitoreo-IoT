import type { Request, Response } from 'express';
import { getHistoricalReadings } from '../services/historicalReadingService.js';

export async function getHistoricalReadingsHandler(request: Request, response: Response) {
  if (request.headers['transfer-encoding'] !== undefined
    || Number(request.headers['content-length'] ?? 0) > 0) {
    response.status(400).json({ mensaje: 'Esta ruta no admite cuerpo en la solicitud.' });
    return;
  }

  const parameters = new URLSearchParams(request.originalUrl.split('?')[1] ?? '');
  for (const key of parameters.keys()) {
    if (!['variable', 'desde', 'hasta'].includes(key)) {
      response.status(400).json({ mensaje: `Parámetro de consulta no permitido: ${key}.` });
      return;
    }
  }

  for (const key of ['variable', 'desde', 'hasta']) {
    if (parameters.getAll(key).length !== 1 || !parameters.get(key)?.trim()) {
      response.status(400).json({ mensaje: `El parámetro ${key} debe ser un texto no vacío y aparecer una sola vez.` });
      return;
    }
  }

  // La validación semántica de variable, fechas ISO 8601 y rango corresponde a #75.
  const filters = {
    variable: parameters.get('variable')!,
    from: parameters.get('desde')!,
    to: parameters.get('hasta')!,
  };
  try {
    response.json(await getHistoricalReadings(filters));
  } catch {
    response.status(500).json({ mensaje: 'No se pudo completar la solicitud.' });
  }
}
