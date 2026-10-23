import type { Pool } from 'pg';
import type { Reading } from '../mqtt/messageValidator.js';

interface Threshold {
  id: string;
  nivel: 'bueno' | 'moderado' | 'malo';
  valor_desde: string;
}

export async function insertCo2Reading(pool: Pool, reading: Reading): Promise<void> {
  const client = await pool.connect();
  try {
    await client.query('BEGIN ISOLATION LEVEL READ COMMITTED');
    // Serializa CO2 entre conexiones y procesos; se libera al terminar la transacción.
    await client.query('SELECT pg_advisory_xact_lock($1, $2)', [90, 1]);
    const previous = await client.query<{ valor: string }>(
      "SELECT valor FROM lecturas WHERE tipo_variable = 'co2' ORDER BY id DESC LIMIT 1",
    );
    const { rows: thresholds } = await client.query<Threshold>(
      "SELECT id, nivel, valor_desde FROM umbrales WHERE tipo_variable = 'co2' ORDER BY valor_desde DESC, id DESC",
    );
    const inserted = await client.query<{ id: string; valor: string }>(
      `INSERT INTO lecturas (fecha_hora, tipo_variable, valor)
       VALUES ($1, $2, $3) RETURNING id, valor`,
      [reading.fecha_hora, reading.tipo_variable, reading.valor],
    );
    const stored = inserted.rows[0];
    // Comparar valores ya redondeados por NUMERIC, igual que las vistas.
    const classify = (value: string) => thresholds.find(t => Number(t.valor_desde) <= Number(value));
    const current = classify(stored.valor);
    const previousLevel = previous.rows[0] && classify(previous.rows[0].valor)?.nivel;
    if (current && current.nivel !== 'bueno' && current.nivel !== previousLevel) {
      await client.query(
        `INSERT INTO alertas (lectura_id, umbral_id, mensaje, atendida)
         VALUES ($1, $2, $3, $4)`,
        [stored.id, current.id,
          `El CO₂ estimado pasó a nivel ${current.nivel}: ${stored.valor} ppm.`, false],
      );
    }
    await client.query('COMMIT');
  } catch (error) {
    await client.query('ROLLBACK').catch(() => undefined);
    throw error;
  } finally {
    client.release();
  }
}
