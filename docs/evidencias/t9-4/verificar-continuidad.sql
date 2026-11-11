-- Solo lectura. Ensayo físico #150 del 29/10/2026, ventana exacta de 5 minutos.
-- Ejecutar con las credenciales privadas habituales; no incluye credenciales.
WITH ventana AS (
  SELECT id, tipo_variable, valor, fecha_hora,
    lag(fecha_hora) OVER (PARTITION BY tipo_variable ORDER BY fecha_hora, id) AS anterior
  FROM lecturas
  WHERE id > 58
    AND fecha_hora >= '2026-10-29T15:27:16.551Z'::timestamptz
    AND fecha_hora < '2026-10-29T15:32:16.551Z'::timestamptz
)
SELECT tipo_variable, count(*) AS guardadas, 20 AS esperadas,
  round(count(*) * 100.0 / 20, 2) AS porcentaje,
  min(fecha_hora) AS primera, max(fecha_hora) AS ultima,
  max(extract(epoch FROM fecha_hora - anterior)) AS max_intervalo_segundos,
  count(*) = 20 AS aprobado
FROM ventana
GROUP BY tipo_variable ORDER BY tipo_variable;
