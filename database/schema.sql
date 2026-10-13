-- Esquema mínimo: una fila por lectura MQTT válida.
-- Ejecutable en DBeaver o psql; no recrea tablas ni índices existentes.
CREATE TABLE IF NOT EXISTS lecturas (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha_hora    TIMESTAMPTZ   NOT NULL DEFAULT now(),   -- guarda zona horaria
    tipo_variable VARCHAR(20)   NOT NULL,                 -- temperatura | humedad | co2
    valor         NUMERIC(8,2)  NOT NULL,                 -- °C, % HR o ppm
    CONSTRAINT chk_lecturas_variable
        CHECK (tipo_variable IN ('temperatura', 'humedad', 'co2'))
);

-- Búsquedas por fecha.
CREATE INDEX IF NOT EXISTS idx_lecturas_fecha_hora
    ON lecturas (fecha_hora);

-- Búsquedas por variable y fecha.
CREATE INDEX IF NOT EXISTS idx_lecturas_variable_fecha
    ON lecturas (tipo_variable, fecha_hora DESC);


-- Verificación opcional de columnas e índices.
-- SELECT column_name, data_type FROM information_schema.columns
-- WHERE table_name = 'lecturas' ORDER BY ordinal_position;
--
-- SELECT indexname FROM pg_indexes
-- WHERE tablename = 'lecturas' ORDER BY indexname;
