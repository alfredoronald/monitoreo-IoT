\set ON_ERROR_STOP on
-- Ejecutar desde la carpeta extraida del paquete; conserva fechas originales.
-- Requiere public.lecturas instalada por el proyecto. No importa calibraciones.
BEGIN;
SET LOCAL lock_timeout = '10s';
SET LOCAL statement_timeout = '120s';
CREATE TEMP TABLE historico_importado (
 id_origen bigint PRIMARY KEY,
 fecha_hora timestamptz NOT NULL,
 tipo_variable varchar(20) NOT NULL CHECK(tipo_variable IN ('temperatura','humedad','co2')),
 valor numeric(8,2) NOT NULL,
 CHECK(tipo_variable <> 'humedad' OR valor BETWEEN 0 AND 100)
) ON COMMIT DROP;
\copy pg_temp.historico_importado (id_origen,fecha_hora,tipo_variable,valor) FROM 'lecturas.csv' WITH (FORMAT csv, HEADER true, ENCODING 'UTF8')
-- FIN_CARGA_CSV
DO $$ BEGIN
 IF (SELECT count(*) FROM pg_temp.historico_importado) <> 10418 THEN
  RAISE EXCEPTION 'Cantidad de filas inesperada; comprobar CSV y manifest';
 END IF;
END $$;
-- Serializa dos importaciones; se recomienda detener la ingesta durante el import.
LOCK TABLE public.lecturas IN SHARE ROW EXCLUSIVE MODE;
WITH nuevas AS (
 INSERT INTO public.lecturas(fecha_hora,tipo_variable,valor)
 SELECT h.fecha_hora,h.tipo_variable,h.valor
 FROM pg_temp.historico_importado h
 WHERE NOT EXISTS (
  SELECT 1 FROM public.lecturas l WHERE l.fecha_hora=h.fecha_hora
    AND l.tipo_variable=h.tipo_variable AND l.valor=h.valor)
 ORDER BY h.id_origen
 RETURNING id
)
SELECT count(*) AS filas_insertadas FROM nuevas;
SELECT count(*) AS filas_del_paquete_presentes FROM pg_temp.historico_importado h
WHERE EXISTS (SELECT 1 FROM public.lecturas l WHERE l.fecha_hora=h.fecha_hora
 AND l.tipo_variable=h.tipo_variable AND l.valor=h.valor);
COMMIT;
