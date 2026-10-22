import type { NextFunction, Request, Response } from 'express';
import { getHistoricalReadings } from '../services/historicalReadingService.js';
import { validateReadingFilters } from './queryValidation.js';

export async function getHistoricalReadingsHandler(request: Request, response: Response, next: NextFunction) {
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
    if (!parameters.has(key)) {
      response.status(400).json({ mensaje: `El parámetro ${key} es obligatorio.` });
      return;
    }
    if (parameters.getAll(key).length !== 1 || !parameters.get(key)?.trim()) {
      response.status(400).json({ mensaje: `El parámetro ${key} debe ser un texto no vacío y aparecer una sola vez.` });
      return;
    }
  }

  const filters = {
    variable: parameters.get('variable')!,
    from: parameters.get('desde')!,
    to: parameters.get('hasta')!,
  };
  const validationError = validateReadingFilters(filters.variable, filters.from, filters.to);
  if (validationError) {
    response.status(400).json({ mensaje: validationError });
    return;
  }
  try {
    response.json(await getHistoricalReadings(filters));
  } catch (error) {
    next(error);
  }
}
