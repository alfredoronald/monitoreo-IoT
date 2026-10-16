import { pool } from '../db.js';
import type { Reading } from '../mqtt/messageValidator.js';

export interface ReadingFilters {
  variable: string;
  from: string;
  to: string;
}

export interface HistoricalReading extends Reading {
  id: string;
}

interface HistoricalReadingRow extends Omit<HistoricalReading, 'valor' | 'fecha_hora'> {
  valor: string;
  fecha_hora: Date;
}

export async function getHistoricalReadings(filters: ReadingFilters): Promise<HistoricalReading[]> {
  const { rows } = await pool.query<HistoricalReadingRow>(
    `SELECT id, tipo_variable, valor, fecha_hora
     FROM lecturas
     WHERE tipo_variable = $1
       AND fecha_hora >= $2::timestamptz
       AND fecha_hora <= $3::timestamptz
     ORDER BY fecha_hora ASC, id ASC`,
    [filters.variable, filters.from, filters.to],
  );

  return rows.map((row) => {
    const value = Number(row.valor);
    if (!Number.isFinite(value)) throw new Error('La lectura contiene un valor no finito.');
    return { id: row.id, tipo_variable: row.tipo_variable, valor: value, fecha_hora: row.fecha_hora.toISOString() };
  });
}
