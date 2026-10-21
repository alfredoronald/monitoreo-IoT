import type { NextFunction, Request, Response } from 'express';
import {
  getAirStatus,
  getAlerts,
  markAlertAsAttended,
  type AlertRow,
  type AirStatusRow,
} from '../services/airStatusService.js';

const MAX_BIGINT = 9223372036854775807n;

function hasRequestBody(request: Request): boolean {
  return request.headers['transfer-encoding'] !== undefined
    || Number(request.headers['content-length'] ?? 0) > 0;
}

function isoDate(value: Date | string): string {
  const date = value instanceof Date ? value : new Date(value);
  return date.toISOString();
}

function serializeAirStatus(row: AirStatusRow | null) {
  if (!row) return null;
  return {
    tipo_variable: row.tipo_variable,
    valor: Number(row.valor),
    fecha_hora: isoDate(row.fecha_hora),
    nivel: row.nivel,
    etiqueta: row.etiqueta,
    recomendacion: row.recomendacion,
  };
}

function serializeAlert(row: AlertRow) {
  return {
    id: String(row.id),
    lectura_id: String(row.lectura_id),
    umbral_id: String(row.umbral_id),
    mensaje: row.mensaje,
    fecha_hora: isoDate(row.fecha_hora),
    atendida: row.atendida,
  };
}

function hasOnlyQuery(request: Request, allowed: string[]): string | undefined {
  const query = new URL(request.originalUrl, 'http://localhost').searchParams;
  for (const key of new Set(query.keys())) {
    if (!allowed.includes(key)) return key;
    if (query.getAll(key).length !== 1) return key;
  }
  return undefined;
}

export async function getAirStatusHandler(request: Request, response: Response, next: NextFunction) {
  const invalidQuery = hasOnlyQuery(request, []);
  if (invalidQuery) {
    response.status(400).json({ mensaje: `Parámetro de consulta no permitido: ${invalidQuery}.` });
    return;
  }
  if (hasRequestBody(request)) {
    response.status(400).json({ mensaje: 'Esta ruta no admite cuerpo en la solicitud.' });
    return;
  }
  try {
    response.json(serializeAirStatus(await getAirStatus()));
  } catch (error) {
    next(error);
  }
}

export async function getAlertsHandler(request: Request, response: Response, next: NextFunction) {
  const invalidQuery = hasOnlyQuery(request, ['atendida']);
  if (invalidQuery) {
    response.status(400).json({ mensaje: `Parámetro de consulta no permitido: ${invalidQuery}.` });
    return;
  }
  if (hasRequestBody(request)) {
    response.status(400).json({ mensaje: 'Esta ruta no admite cuerpo en la solicitud.' });
    return;
  }

  const query = new URL(request.originalUrl, 'http://localhost').searchParams;
  const attendedText = query.get('atendida');
  if (attendedText !== null && attendedText !== 'true' && attendedText !== 'false') {
    response.status(400).json({ mensaje: 'atendida debe ser true o false.' });
    return;
  }

  try {
    const alerts = await getAlerts(attendedText === null ? undefined : attendedText === 'true');
    response.json(alerts.map(serializeAlert));
  } catch (error) {
    next(error);
  }
}

export async function patchAlertHandler(request: Request, response: Response, next: NextFunction) {
  if (!request.is('application/json')) {
    response.status(415).json({ mensaje: 'Se requiere Content-Type: application/json.' });
    return;
  }
  if (hasOnlyQuery(request, [])) {
    response.status(400).json({ mensaje: 'Esta ruta no admite parámetros de consulta.' });
    return;
  }

  const { id } = request.params;
  if (!/^\d+$/.test(id) || BigInt(id) < 1n || BigInt(id) > MAX_BIGINT) {
    response.status(400).json({ mensaje: 'id debe ser un entero positivo dentro del rango BIGINT.' });
    return;
  }

  const body = request.body as unknown;
  if (body === null || typeof body !== 'object' || Array.isArray(body)
    || Object.keys(body).length !== 1 || (body as { atendida?: unknown }).atendida !== true) {
    response.status(400).json({ mensaje: 'El cuerpo debe contener únicamente atendida con valor true.' });
    return;
  }

  try {
    const alert = await markAlertAsAttended(id);
    if (!alert) {
      response.status(404).json({ mensaje: 'No se encontró la alerta solicitada.' });
      return;
    }
    response.json(serializeAlert(alert));
  } catch (error) {
    next(error);
  }
}
