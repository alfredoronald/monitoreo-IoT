-- Esquema completo para PostgreSQL 15+: tablas, índices, vistas y RLS.
-- No recrea tablas existentes; actualiza las vistas.

-- Una fila por lectura MQTT válida.
CREATE TABLE IF NOT EXISTS lecturas (
    id             BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha_hora     TIMESTAMPTZ  NOT NULL DEFAULT now(),   -- guarda zona horaria
    tipo_variable  VARCHAR(20)  NOT NULL,                 -- temperatura | humedad | co2
    valor          NUMERIC(8,2) NOT NULL,                 -- °C, % HR o ppm
    CONSTRAINT chk_lecturas_variable
        CHECK (tipo_variable IN ('temperatura', 'humedad', 'co2')),
    CONSTRAINT chk_lecturas_humedad
        CHECK (tipo_variable <> 'humedad' OR valor BETWEEN 0 AND 100)
);

-- Búsquedas por fecha.
CREATE INDEX IF NOT EXISTS idx_lecturas_fecha_hora
    ON lecturas (fecha_hora);
-- Búsquedas por variable y fecha.
CREATE INDEX IF NOT EXISTS idx_lecturas_variable_fecha
    ON lecturas (tipo_variable, fecha_hora DESC);


-- Resultados de pruebas: mensajes enviados, recibidos y guardados.
CREATE TABLE IF NOT EXISTS pruebas_transmision (
    id             INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha_hora     TIMESTAMPTZ  NOT NULL DEFAULT now(),
    descripcion    TEXT         NOT NULL,            -- ej. "Carga de 100 mensajes (T-2.8)"
    duracion_min   NUMERIC(6,1),
    qos            SMALLINT,                         -- 0 o 1
    intervalo_s    SMALLINT,                         -- intervalo de publicación usado
    enviados       INTEGER      NOT NULL CHECK (enviados  >= 0),
    recibidos      INTEGER      NOT NULL CHECK (recibidos >= 0),
    guardados      INTEGER      NOT NULL CHECK (guardados >= 0),
    perdidos_pct   NUMERIC(5,2) GENERATED ALWAYS AS (
                       CASE WHEN enviados > 0
                            THEN round(100.0 * (enviados - guardados) / enviados, 2)
                       END
                   ) STORED,
    observaciones  TEXT,
    CONSTRAINT chk_pruebas_qos CHECK (qos IS NULL OR qos IN (0, 1))
);


-- Cada nivel comienza en valor_desde; se aplica el mayor que no supere la lectura.
CREATE TABLE IF NOT EXISTS umbrales (
    id             INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    tipo_variable  VARCHAR(20) NOT NULL,
    nivel          VARCHAR(20) NOT NULL,
    valor_desde    NUMERIC(8,2) NOT NULL,
    etiqueta       VARCHAR(60) NOT NULL,       -- texto para el indicador del dashboard
    recomendacion  TEXT,
    CONSTRAINT chk_umbrales_variable
        CHECK (tipo_variable IN ('temperatura', 'humedad', 'co2')),
    CONSTRAINT chk_umbrales_nivel
        CHECK (nivel IN ('bueno', 'moderado', 'malo')),
    CONSTRAINT uq_umbrales_variable_nivel UNIQUE (tipo_variable, nivel)
);

-- Umbrales provisionales de CO2, pendientes de calibración.
INSERT INTO umbrales (tipo_variable, nivel, valor_desde, etiqueta, recomendacion) VALUES
    ('co2', 'bueno',       0, 'Aire bueno',     'No hace falta ventilar.'),
    ('co2', 'moderado',  800, 'Aire moderado',  'Conviene ventilar pronto.'),
    ('co2', 'malo',     1200, 'Aire malo',      'Ventila el aula ahora.')
ON CONFLICT (tipo_variable, nivel) DO NOTHING;


-- Alertas asociadas a lecturas y umbrales.
CREATE TABLE IF NOT EXISTS alertas (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    lectura_id  BIGINT      NOT NULL REFERENCES lecturas(id) ON DELETE CASCADE,
    umbral_id   INTEGER     NOT NULL REFERENCES umbrales(id) ON DELETE RESTRICT,
    mensaje     TEXT        NOT NULL,
    fecha_hora  TIMESTAMPTZ NOT NULL DEFAULT now(),
    atendida    BOOLEAN     NOT NULL DEFAULT false
);

CREATE INDEX IF NOT EXISTS idx_alertas_lectura
    ON alertas (lectura_id);
CREATE INDEX IF NOT EXISTS idx_alertas_pendientes
    ON alertas (fecha_hora DESC) WHERE atendida = false;


-- Historial de calibraciones del MQ-135.
CREATE TABLE IF NOT EXISTS calibraciones (
    id                       INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    sensor                   VARCHAR(20)  NOT NULL DEFAULT 'MQ-135',
    fecha_hora               TIMESTAMPTZ  NOT NULL DEFAULT now(),
    ro_kohm                  NUMERIC(10,3) NOT NULL CHECK (ro_kohm > 0),   -- resistencia en aire limpio
    rl_kohm                  NUMERIC(10,3) CHECK (rl_kohm > 0),            -- resistencia de carga del módulo
    co2_referencia_ppm       NUMERIC(8,2) NOT NULL DEFAULT 400,            -- aire exterior de referencia
    acondicionamiento_horas  NUMERIC(6,1),                                 -- 24 h o más (T-1.1)
    vigente                  BOOLEAN     NOT NULL DEFAULT false,
    observaciones            TEXT
);

-- Garantiza una sola calibración vigente por sensor
CREATE UNIQUE INDEX IF NOT EXISTS uq_calibracion_vigente
    ON calibraciones (sensor) WHERE vigente;


-- Vistas para el dashboard; usan los permisos del usuario que consulta.
CREATE OR REPLACE VIEW v_ultima_lectura
WITH (security_invoker = true) AS
SELECT DISTINCT ON (tipo_variable)
       tipo_variable, valor, fecha_hora
FROM lecturas
ORDER BY tipo_variable, fecha_hora DESC;

CREATE OR REPLACE VIEW v_estado_aire
WITH (security_invoker = true) AS
SELECT l.tipo_variable, l.valor, l.fecha_hora,
       u.nivel, u.etiqueta, u.recomendacion
FROM v_ultima_lectura l
JOIN LATERAL (
    SELECT x.nivel, x.etiqueta, x.recomendacion
    FROM umbrales x
    WHERE x.tipo_variable = l.tipo_variable
      AND x.valor_desde <= l.valor
    ORDER BY x.valor_desde DESC
    LIMIT 1
) u ON true;


-- RLS: el dueño normalmente la omite; otros roles requieren políticas explícitas.
ALTER TABLE lecturas             ENABLE ROW LEVEL SECURITY;
ALTER TABLE pruebas_transmision  ENABLE ROW LEVEL SECURITY;
ALTER TABLE umbrales             ENABLE ROW LEVEL SECURITY;
ALTER TABLE alertas              ENABLE ROW LEVEL SECURITY;
ALTER TABLE calibraciones        ENABLE ROW LEVEL SECURITY;


-- Datos sintéticos opcionales: 240 lecturas por variable, separadas por 15 s.
-- INSERT INTO lecturas (fecha_hora, tipo_variable, valor)
-- SELECT now() - (g * interval '15 seconds'),
--        v.nombre,
--        round((v.base + v.amplitud * sin(g / 20.0) + (random() - 0.5))::numeric, 2)
-- FROM generate_series(0, 239) AS g
-- CROSS JOIN (VALUES
--     ('temperatura', 22.0,   3.0),
--     ('humedad',     55.0,  10.0),
--     ('co2',        700.0, 300.0)
-- ) AS v(nombre, base, amplitud);


-- Verificación opcional: cinco tablas y dos vistas del proyecto.
-- SELECT table_name, table_type FROM information_schema.tables
-- WHERE table_schema = 'public' ORDER BY table_type, table_name;
