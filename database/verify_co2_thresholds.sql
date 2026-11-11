-- #88: ejecutar el script completo como dueño de las tablas, sin ingestión activa.
-- Requiere database/esquema.sql. Procedimiento: docs/evidencias/umbrales-co2-88.md.
-- Ante un error, ejecutar ROLLBACK para liberar la transacción.

BEGIN;
SET LOCAL lock_timeout = '5s';
SET LOCAL statement_timeout = '30s';
LOCK TABLE public.lecturas, public.umbrales IN SHARE ROW EXCLUSIVE MODE;

SELECT nivel, valor_desde, etiqueta, recomendacion
FROM public.umbrales
WHERE tipo_variable = 'co2'
ORDER BY valor_desde;

CREATE TEMP TABLE co2_threshold_results (
    orden INTEGER PRIMARY KEY,
    caso TEXT NOT NULL,
    valor NUMERIC(8,2) NOT NULL,
    umbral_moderado NUMERIC(8,2) NOT NULL,
    nivel_esperado TEXT NOT NULL,
    nivel_obtenido TEXT NOT NULL,
    etiqueta TEXT NOT NULL,
    recomendacion TEXT,
    resultado TEXT NOT NULL
) ON COMMIT DROP;

DO $$
DECLARE
    sample_id BIGINT;
    sample_time TIMESTAMPTZ;
    test_case RECORD;
    actual RECORD;
    threshold RECORD;
    matching_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO matching_count
    FROM public.umbrales
    WHERE tipo_variable = 'co2'
      AND (nivel, valor_desde) IN (
          ('bueno', 0), ('moderado', 800), ('malo', 1200)
      );
    IF matching_count <> 3 THEN
        RAISE EXCEPTION 'Se requieren los umbrales provisionales 0/800/1200; revisar con el PO antes de adaptar la prueba';
    END IF;

    -- Id explícito para no consumir la secuencia; fecha posterior para probar la última lectura.
    SELECT LEAST(COALESCE(MIN(id), 0), 0) - 1 INTO sample_id
    FROM public.lecturas;
    SELECT GREATEST(clock_timestamp(), COALESCE(MAX(fecha_hora), clock_timestamp()))
           + INTERVAL '1 second' INTO sample_time
    FROM public.lecturas WHERE tipo_variable = 'co2';
    INSERT INTO public.lecturas (id, fecha_hora, tipo_variable, valor)
    OVERRIDING SYSTEM VALUE
    VALUES (sample_id, sample_time, 'co2', 0);

    FOR test_case IN
        SELECT * FROM (VALUES
            (1, 'Inicio de bueno',             0.00,    'bueno',    800.00),
            (2, 'Antes de moderado',         799.99,    'bueno',    800.00),
            (3, 'Inicio de moderado',        800.00,    'moderado', 800.00),
            (4, 'Antes de malo',            1199.99,    'moderado', 800.00),
            (5, 'Inicio de malo',           1200.00,    'malo',     800.00),
            (6, 'Por encima de malo',       1500.00,    'malo',     800.00),
            (7, '825 con umbral original',   825.00,    'moderado', 800.00),
            (8, '825 con umbral modificado', 825.00,    'bueno',    850.00),
            (9, 'Nuevo inicio de moderado',  850.00,    'moderado', 850.00)
        ) AS cases(orden, caso, valor, esperado, umbral_moderado)
        ORDER BY orden
    LOOP
        UPDATE public.umbrales SET valor_desde = test_case.umbral_moderado
        WHERE tipo_variable = 'co2' AND nivel = 'moderado';
        UPDATE public.lecturas SET valor = test_case.valor WHERE id = sample_id;

        SELECT * INTO STRICT actual
        FROM public.v_estado_aire WHERE tipo_variable = 'co2';
        SELECT * INTO STRICT threshold
        FROM public.umbrales
        WHERE tipo_variable = 'co2' AND nivel = test_case.esperado;

        IF actual.valor IS DISTINCT FROM test_case.valor
           OR actual.fecha_hora IS DISTINCT FROM sample_time
           OR actual.nivel IS DISTINCT FROM test_case.esperado
           OR actual.etiqueta IS DISTINCT FROM threshold.etiqueta
           OR actual.recomendacion IS DISTINCT FROM threshold.recomendacion THEN
            RAISE EXCEPTION 'Caso % falló: valor %, nivel esperado %, obtenido %',
                test_case.orden, test_case.valor, test_case.esperado, actual.nivel;
        END IF;

        INSERT INTO co2_threshold_results VALUES (
            test_case.orden, test_case.caso, actual.valor,
            test_case.umbral_moderado, test_case.esperado, actual.nivel,
            actual.etiqueta, actual.recomendacion, 'PASS'
        );
    END LOOP;
END
$$;

SELECT * FROM co2_threshold_results ORDER BY orden;

ROLLBACK;

SELECT nivel, valor_desde, etiqueta, recomendacion
FROM public.umbrales
WHERE tipo_variable = 'co2'
ORDER BY valor_desde;
