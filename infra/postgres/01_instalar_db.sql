-- =====================================================================
-- Monitoreo IoT de Calidad Ambiental · Grupo 22
-- Instalación: base de datos de la aplicación y usuario propio
--
-- Se ejecuta como superusuario (postgres), desde psql o DBeaver:
--   psql -U postgres -h localhost -f infra/postgres/01_instalar_db.sql
--
-- Crea:
--   * el rol app (login, sin privilegios de más)
--   * la base monitoreo_ambiental, de la que app es dueño
--   * y deja app SIN acceso a las demás bases
--
-- La contraseña de app NO va en este archivo (regla: nunca credential-
-- bles versionadas). Se define después en local, ver README.md.
--
-- Se puede ejecutar más de una vez sin errores (bloques IF NOT EXISTS).
-- =====================================================================


-- =====================================================================
-- 1. Rol de la aplicación
-- NOSUPERUSER NOCREATEDB NOCREATEROLE NOBYPASSRLS: solo puede conectarse
-- y trabajar dentro de su propia base.
-- =====================================================================
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'app') THEN
        CREATE ROLE app
            LOGIN
            NOSUPERUSER
            NOCREATEDB
            NOCREATEROLE
            NOBYPASSRLS;
    END IF;
END
$$;


-- =====================================================================
-- 2. Base de datos de la aplicación (dueño: app)
-- ⚠️ CREATE DATABASE no admite IF NOT EXISTS ni ejecutarse dentro de una
-- transacción. Si la base ya existe, PostgreSQL responde con el error
-- 42P04 ("database already exists"): es esperado, continúa con el
-- bloque 3.
-- =====================================================================
CREATE DATABASE monitoreo_ambiental
    OWNER app
    ENCODING 'UTF8';


-- =====================================================================
-- 3. Permisos: la app solo ve su base
-- CONNECT está concedido a PUBLIC por defecto en todas las bases; aquí
-- se revoca de las demás y se concede (explícitamente) en la suya.
-- =====================================================================
DO $$
DECLARE
    db RECORD;
BEGIN
    FOR db IN
        SELECT datname FROM pg_database
        WHERE datname NOT IN ('monitoreo_ambiental', 'template0')
    LOOP
        EXECUTE format('REVOKE CONNECT ON DATABASE %I FROM app', db.datname);
    END LOOP;
END
$$;

GRANT CONNECT ON DATABASE monitoreo_ambiental TO app;


-- =====================================================================
-- VERIFICACIÓN (correr como postgres): debe devolver 1 fila con
-- datname = monitoreo_ambiental y datdba = app
-- =====================================================================
-- SELECT d.datname, r.rolname AS owner
-- FROM pg_database d JOIN pg_roles r ON r.oid = d.datdba
-- WHERE d.datname = 'monitoreo_ambiental';
