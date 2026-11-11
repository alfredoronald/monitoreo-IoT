-- =====================================================================
-- Monitoreo IoT de Calidad Ambiental · HU-03
-- Datos de prueba para validar consultas por rango de fechas.
--
-- Requisito previo: ejecutar database/schema.sql o database/esquema.sql.
-- Compatible con PostgreSQL local y con el SQL Editor de Supabase.
-- =====================================================================

BEGIN;

-- Cinco lecturas de cada variable en un intervalo de cuatro horas.
-- Los valores son intencionalmente deterministas para facilitar la
-- evidencia y la comparación entre ejecuciones de la consulta.
INSERT INTO lecturas (fecha_hora, tipo_variable, valor)
VALUES
    (CURRENT_TIMESTAMP - INTERVAL '4 hours',       'temperatura', 21.80),
    (CURRENT_TIMESTAMP - INTERVAL '3 hours',       'temperatura', 22.40),
    (CURRENT_TIMESTAMP - INTERVAL '2 hours',       'temperatura', 23.10),
    (CURRENT_TIMESTAMP - INTERVAL '1 hour',        'temperatura', 22.70),
    (CURRENT_TIMESTAMP - INTERVAL '15 minutes',    'temperatura', 22.20),

    (CURRENT_TIMESTAMP - INTERVAL '4 hours',       'humedad',     52.00),
    (CURRENT_TIMESTAMP - INTERVAL '3 hours',       'humedad',     55.50),
    (CURRENT_TIMESTAMP - INTERVAL '2 hours',       'humedad',     58.00),
    (CURRENT_TIMESTAMP - INTERVAL '1 hour',        'humedad',     56.50),
    (CURRENT_TIMESTAMP - INTERVAL '15 minutes',    'humedad',     54.00),

    (CURRENT_TIMESTAMP - INTERVAL '4 hours',       'co2',         480.00),
    (CURRENT_TIMESTAMP - INTERVAL '3 hours',       'co2',         620.00),
    (CURRENT_TIMESTAMP - INTERVAL '2 hours',       'co2',         780.00),
    (CURRENT_TIMESTAMP - INTERVAL '1 hour',        'co2',         910.00),
    (CURRENT_TIMESTAMP - INTERVAL '15 minutes',    'co2',         700.00);

COMMIT;

-- =====================================================================
-- Evidencia: consulta por rango de fechas.
-- Debe devolver 15 filas: 5 por cada variable.
-- =====================================================================
SELECT
    tipo_variable,
    COUNT(*) AS cantidad,
    MIN(fecha_hora) AS primera_lectura,
    MAX(fecha_hora) AS ultima_lectura
FROM lecturas
WHERE fecha_hora >= CURRENT_TIMESTAMP - INTERVAL '5 hours'
  AND fecha_hora <= CURRENT_TIMESTAMP
  AND tipo_variable IN ('temperatura', 'humedad', 'co2')
GROUP BY tipo_variable
ORDER BY tipo_variable;

-- Detalle que puede utilizarse como captura de pantalla de la evidencia.
SELECT id, fecha_hora, tipo_variable, valor
FROM lecturas
WHERE fecha_hora >= CURRENT_TIMESTAMP - INTERVAL '5 hours'
  AND fecha_hora <= CURRENT_TIMESTAMP
  AND tipo_variable IN ('temperatura', 'humedad', 'co2')
ORDER BY fecha_hora ASC, tipo_variable ASC;

