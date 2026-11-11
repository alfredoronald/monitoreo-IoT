import { pool } from '../db.js';
import type { Reading, ReadingVariable } from '../mqtt/messageValidator.js';

interface LatestReadingRow {
  tipo_variable: ReadingVariable;
  valor: string;
  fecha_hora: Date;
}

export async function getLatestReadings(): Promise<Reading[]> {
  const { rows } = await pool.query<LatestReadingRow>(
    `SELECT tipo_variable, valor, fecha_hora
     FROM v_ultima_lectura
     ORDER BY tipo_variable ASC`,
  );

  return rows.map((row) => {
    const value = Number(row.valor);
    if (!Number.isFinite(value)) {
      throw new Error('La lectura contiene un valor no finito.');
    }

    return {
      tipo_variable: row.tipo_variable,
      valor: value,
      fecha_hora: row.fecha_hora.toISOString(),
    };
  });
}
