import { pool } from '../db.js';

export type AirStatusRow = {
  tipo_variable: string;
  valor: string | number;
  fecha_hora: Date | string;
  nivel: string;
  etiqueta: string;
  recomendacion: string | null;
};

export type AlertRow = {
  id: string | number;
  lectura_id: string | number;
  umbral_id: string | number;
  mensaje: string;
  fecha_hora: Date | string;
  atendida: boolean;
};

export async function getAirStatus(): Promise<AirStatusRow | null> {
  const result = await pool.query<AirStatusRow>(
    `SELECT tipo_variable, valor, fecha_hora, nivel, etiqueta, recomendacion
     FROM v_estado_aire
     WHERE tipo_variable = 'co2'
     LIMIT 1`,
  );
  return result.rows[0] ?? null;
}

export async function getAlerts(attended?: boolean): Promise<AlertRow[]> {
  const condition = attended === undefined ? '' : 'WHERE atendida = $1';
  const values = attended === undefined ? [] : [attended];
  const result = await pool.query<AlertRow>(
    `SELECT id, lectura_id, umbral_id, mensaje, fecha_hora, atendida
     FROM alertas
     ${condition}
     ORDER BY fecha_hora DESC, id DESC`,
    values,
  );
  return result.rows;
}

export async function markAlertAsAttended(id: string): Promise<AlertRow | null> {
  const result = await pool.query<AlertRow>(
    `UPDATE alertas
     SET atendida = true
     WHERE id = $1
     RETURNING id, lectura_id, umbral_id, mensaje, fecha_hora, atendida`,
    [id],
  );
  return result.rows[0] ?? null;
}
