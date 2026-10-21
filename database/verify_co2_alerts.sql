-- #90: ejecutar en la base/esquema donde se publicaron las lecturas de prueba.
-- Secuencia CO2: 700, 850, 900, 1250, 1300, 950, 700, 850.
-- Esperado en un esquema vacío: ocho lecturas CO2 y cuatro alertas sin atender.
SELECT a.id, a.lectura_id, l.valor AS co2_ppm, u.nivel,
       a.atendida, a.mensaje, a.fecha_hora
FROM alertas a
JOIN lecturas l ON l.id = a.lectura_id
JOIN umbrales u ON u.id = a.umbral_id
ORDER BY a.id;

SELECT COUNT(*) AS alertas, COUNT(*) FILTER (WHERE NOT atendida) AS sin_atender
FROM alertas;

-- Verificar lecturas, incluidas las que no generaron alerta.
SELECT l.id, l.tipo_variable, l.valor, l.fecha_hora,
       a.id AS alerta_id, u.nivel AS nivel_alertado
FROM lecturas l
LEFT JOIN alertas a ON a.lectura_id = l.id
LEFT JOIN umbrales u ON u.id = a.umbral_id
ORDER BY l.id;
