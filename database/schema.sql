-- =====================================================================
-- Monitoreo IoT de Calidad Ambiental · Grupo 22 · Lecturas (HU-03)
-- Script mínimo: tabla lecturas + índices para las consultas del dashboard.
-- Para ejecutar en DBeaver o psql sobre una base vacía.
--
-- Convive con database/esquema.sql (mismos nombres de índices y columnas
-- + IF NOT EXISTS): se puede ejecutar en cualquier orden y más de una vez
-- sin errores y sin duplicar tablas ni índices.
-- =====================================================================


-- Una fila por cada mensaje MQTT válido que guarda el backend.
-- Campos del plan: id, fecha_hora, tipo_variable, valor.
CREATE TABLE IF NOT EXISTS lecturas (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha_hora    TIMESTAMPTZ   NOT NULL DEFAULT now(),   -- guarda zona horaria
    tipo_variable VARCHAR(20)   NOT NULL,                 -- temperatura | humedad | co2
    valor         NUMERIC(8,2)  NOT NULL,                 -- °C, % HR o ppm
    CONSTRAINT chk_lecturas_variable
        CHECK (tipo_variable IN ('temperatura', 'humedad', 'co2'))
);

-- Consultas por rango de fechas del dashboard (GET /api/lecturas?desde=&hasta=)
CREATE INDEX IF NOT EXISTS idx_lecturas_fecha_hora
    ON lecturas (fecha_hora);

-- GET /api/lecturas?variable=&desde=&hasta= y "última lectura por variable"
CREATE INDEX IF NOT EXISTS idx_lecturas_variable_fecha
    ON lecturas (tipo_variable, fecha_hora DESC);


-- =====================================================================
-- VERIFICACIÓN: tabla lecturas + sus 2 índices, fecha_hora en TIMESTAMPTZ
-- =====================================================================
-- SELECT column_name, data_type FROM information_schema.columns
-- WHERE table_name = 'lecturas' ORDER BY ordinal_position;
--
-- SELECT indexname FROM pg_indexes
-- WHERE tablename = 'lecturas' ORDER BY indexname;
