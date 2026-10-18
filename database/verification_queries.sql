-- #29: ejecutar en la base monitoreo_ambiental con DBeaver o psql.
-- Conteo histórico, incluidos datos semilla y pruebas anteriores.
SELECT v.tipo_variable, COUNT(l.id) AS cantidad
FROM (VALUES ('temperatura'), ('humedad'), ('co2')) AS v(tipo_variable)
LEFT JOIN lecturas l ON l.tipo_variable = v.tipo_variable
GROUP BY v.tipo_variable
ORDER BY v.tipo_variable;

-- Últimas 20 lecturas; id desempata fechas iguales.
SELECT id, tipo_variable, valor, fecha_hora
FROM lecturas
ORDER BY fecha_hora DESC, id DESC
LIMIT 20;

-- Última lectura disponible de cada variable.
SELECT DISTINCT ON (tipo_variable) id, tipo_variable, valor, fecha_hora
FROM lecturas
ORDER BY tipo_variable, fecha_hora DESC, id DESC;

-- Ejecutar antes y después de publicar para registrar los límites de la prueba.
SELECT COALESCE(MAX(id), 0) AS limite_id FROM lecturas;

-- Sustituir NULL por los límites inicial y final registrados en la prueba.
-- Ejecutar SELECT COALESCE(MAX(id), 0) FROM lecturas en ambos momentos:
-- antes de publicar (baseline_id) y después de completar las inserciones (final_id).
-- Ejemplo ilustrativo: si los resultados son 123 y 456, usar en la CTE:
-- SELECT 123::bigint AS baseline_id, 456::bigint AS final_id
-- No debe haber otras fuentes insertando lecturas durante la publicación.
WITH limits AS (
    SELECT NULL::bigint AS baseline_id, NULL::bigint AS final_id
), expected AS (
    SELECT * FROM (VALUES
        ('temperatura', 5), ('humedad', 5), ('co2', 5)
    ) AS v(tipo_variable, publicados_validos)
)
SELECT e.tipo_variable, e.publicados_validos,
       COUNT(l.id) AS registrados,
       CASE WHEN b.baseline_id IS NULL OR b.final_id IS NULL
                  OR b.final_id < b.baseline_id THEN 'CONFIGURAR LIMITES'
            WHEN COUNT(l.id) = e.publicados_validos THEN 'COINCIDE'
            ELSE 'NO COINCIDE' END AS resultado
FROM expected e
CROSS JOIN limits b
LEFT JOIN lecturas l ON l.tipo_variable = e.tipo_variable
    AND l.id > b.baseline_id AND l.id <= b.final_id
GROUP BY e.tipo_variable, e.publicados_validos, b.baseline_id, b.final_id
ORDER BY e.tipo_variable;
