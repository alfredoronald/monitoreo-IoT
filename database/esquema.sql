-- =====================================================================
-- Monitoreo IoT de Calidad Ambiental · Grupo 22 · Esquema del proyecto completo
-- Para ejecutar en DBeaver sobre una conexión PostgreSQL.
--
--   Sprint 1 (HU-01 a HU-03)  lecturas, pruebas_transmision
--   Sprint 2 (HU-04 a HU-06)  umbrales, alertas, vistas para la API y el dashboard
--   Sprint 3 (HU-07 a HU-09)  calibraciones (HU-08), pruebas_transmision (HU-09)
--
-- HU-07 (clave de la API) no necesita tabla: API_KEY va en el .env del backend.
-- Se puede ejecutar más de una vez sin errores (IF NOT EXISTS / OR REPLACE).
-- =====================================================================


-- =====================================================================
-- SPRINT 1 · LECTURAS (HU-03)
-- Una fila por cada mensaje MQTT válido que guarda el backend.
-- Campos del plan: id, fecha_hora, tipo_variable, valor.
-- =====================================================================
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

-- CA 3 de HU-03: consultas por rango de fechas rápidas
CREATE INDEX IF NOT EXISTS idx_lecturas_fecha_hora
    ON lecturas (fecha_hora);
-- Para GET /api/lecturas?variable=&desde=&hasta= y "última lectura por variable"
CREATE INDEX IF NOT EXISTS idx_lecturas_variable_fecha
    ON lecturas (tipo_variable, fecha_hora DESC);


-- =====================================================================
-- SPRINT 1 y 3 · PRUEBAS_TRANSMISION (T-1.8, T-2.8, T-4.1 y HU-09)
-- Guarda las tablas "enviados / recibidos / guardados" de cada prueba,
-- para citarlas en el informe sin buscar capturas sueltas.
-- =====================================================================
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


-- =====================================================================
-- SPRINT 2 · UMBRALES (HU-06)
-- Rangos que definen el estado del aire. Cada fila indica desde qué valor
-- empieza un nivel; el nivel vigente es el de mayor valor_desde que no
-- supere la lectura. Se editan sin tocar el código.
-- =====================================================================
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

-- Valores iniciales PROVISIONALES para CO2 (se ajustan tras la calibración de HU-08)
INSERT INTO umbrales (tipo_variable, nivel, valor_desde, etiqueta, recomendacion) VALUES
    ('co2', 'bueno',       0, 'Aire bueno',     'No hace falta ventilar.'),
    ('co2', 'moderado',  800, 'Aire moderado',  'Conviene ventilar pronto.'),
    ('co2', 'malo',     1200, 'Aire malo',      'Ventila el aula ahora.')
ON CONFLICT (tipo_variable, nivel) DO NOTHING;


-- =====================================================================
-- SPRINT 2 · ALERTAS (HU-06)
-- Se genera una fila cuando una lectura entra en un nivel "moderado" o "malo".
-- =====================================================================
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


-- =====================================================================
-- SPRINT 3 · CALIBRACIONES (HU-08)
-- Historial de calibraciones del MQ-135. Solo una puede estar vigente;
-- el backend o el firmware toman el Ro de esa fila.
-- =====================================================================
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


-- =====================================================================
-- SPRINT 2 · VISTAS PARA LA API Y EL DASHBOARD (HU-04, HU-05, HU-06)
-- security_invoker: válido en PostgreSQL 15+ (no es exclusivo de Supabase).
-- =====================================================================
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


-- =====================================================================
-- SEGURIDAD (RLS): opcional para uso local en DBeaver.
-- Row Level Security no bloquea al dueño de las tablas (el usuario con el
-- que te conectas normalmente desde DBeaver), así que puedes dejar esto
-- activado sin que te impida leer/escribir en desarrollo local. Tiene
-- sentido real cuando expongas la base a través de la API pública de
-- Supabase (rol "anon") — ahí sí necesitarás políticas explícitas.
-- Si prefieres omitirlo por ahora, comenta este bloque.
-- =====================================================================
ALTER TABLE lecturas             ENABLE ROW LEVEL SECURITY;
ALTER TABLE pruebas_transmision  ENABLE ROW LEVEL SECURITY;
ALTER TABLE umbrales             ENABLE ROW LEVEL SECURITY;
ALTER TABLE alertas              ENABLE ROW LEVEL SECURITY;
ALTER TABLE calibraciones        ENABLE ROW LEVEL SECURITY;


-- =====================================================================
-- OPCIONAL · T-3.3 Datos de prueba (240 lecturas por variable, cada 15 s)
-- Descomentar para usarlo. Antes de las pruebas reales, vacía la tabla con:
--   TRUNCATE lecturas RESTART IDENTITY CASCADE;
-- =====================================================================
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


-- =====================================================================
-- VERIFICACIÓN: deben salir las 5 tablas y las 2 vistas
-- =====================================================================
-- SELECT table_name, table_type FROM information_schema.tables
-- WHERE table_schema = 'public' ORDER BY table_type, table_name;
