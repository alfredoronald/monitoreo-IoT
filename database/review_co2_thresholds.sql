-- T-8.7 / #146: consulta y limites; no modifica umbrales ni lecturas.
BEGIN READ ONLY;
SET LOCAL statement_timeout = '10s';
SELECT id, tipo_variable, nivel, valor_desde, etiqueta, recomendacion
FROM public.umbrales WHERE tipo_variable = 'co2' ORDER BY valor_desde, id;

-- Expectativa de la propuesta de conservar 0/800/1200; no es una medicion fisica.
WITH casos(valor, esperado) AS (VALUES
 (0.00::numeric, 'bueno'), (799.99, 'bueno'), (800.00, 'moderado'),
 (1199.99, 'moderado'), (1200.00, 'malo'), (1500.00, 'malo'))
SELECT c.valor, c.esperado, u.nivel AS obtenido,
       COALESCE(u.nivel = c.esperado, false) AS coincide
FROM casos c LEFT JOIN LATERAL (
 SELECT nivel FROM public.umbrales
 WHERE tipo_variable = 'co2' AND valor_desde <= c.valor
 ORDER BY valor_desde DESC, id DESC LIMIT 1
) u ON true ORDER BY c.valor;
COMMIT;
