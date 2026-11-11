# PostgreSQL local — instalación

Configuración de la base de datos del monitoreo ambiental: crear la base
`monitoreo_ambiental` y el usuario propio de la aplicación (`app`) con
permisos **solo** sobre esa base.

> Instalación local de PostgreSQL **sin Docker**, igual que el resto del
> proyecto (sección 6.3 del informe).

## 1. Instalar PostgreSQL

1. Descargar el instalador oficial (<https://www.postgresql.org/download/windows/>).
2. Durante la instalación se define la contraseña del superusuario
   **`postgres`** (guárdala: la necesitas para cada paso de este README).
3. Puerto por defecto: **5432** (dejarlo).
4. Verificar que el servicio esté corriendo: `services.msc` →
   `postgresql-x64-18` → *Iniciar*.

## 2. Crear la base y el usuario

Ejecutar [`01_instalar_db.sql`](01_instalar_db.sql) **conectado como
superusuario**:

```powershell
# psql viene en C:\Program Files\PostgreSQL\<versión>\bin (agregarlo al PATH)
psql -U postgres -h localhost -f infra/postgres/01_instalar_db.sql
```

Lo que hace el script (y se puede volver a ejecutar sin errores):

| Paso | Efecto |
|---|---|
| Rol `app` | `LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOBYPASSRLS` |
| Base `monitoreo_ambiental` | creada con **dueño `app`** |
| Permisos | `app` tiene `CONNECT` solo en su base; se revoca en las demás |

> Si la base ya existe, el script avisa con el error `42P04` en la línea
> del `CREATE DATABASE`: es esperado, no corta el resto.

**Importante:** el dueño de las tablas debe ser `app`, y por eso el dueño
de la base es `app`. Si creas las tablas conectado como `postgres`, el
RLS activado en `database/esquema.sql` hace que `app` lea **0 filas**
(RLS no bloquea al dueño, y el dueño sería `postgres`).

## 3. Asignar la contraseña de `app`

La contraseña **no está en ningún archivo del repo**. Se define una sola
vez, en local, y se guarda solo en `backend/.env`:

```sql
-- correr como postgres
ALTER ROLE app PASSWORD 'aqui_va_tu_contraseña_local';
```

Para cambiarla después: mismo comando con el valor nuevo.

## 4. Conexión desde DBeaver

*Archivo → Nuevo → Conexión → PostgreSQL:*

| Campo | Valor |
|---|---|
| Host | `localhost` |
| Puerto | `5432` |
| Base de datos | `monitoreo_ambiental` |
| Usuario | `app` |
| Contraseña | la definida en el paso 3 |
| Esquema | `public` |

Comprobación de que los permisos son los correctos (correr como `app`):

```sql
-- debe listar las tablas del proyecto
SELECT table_name FROM information_schema.tables
WHERE table_schema = 'public' ORDER BY table_name;

-- debe fallar con "permission denied"
CREATE DATABASE prueba;
```

## 5. Cargar el esquema

Con la **misma conexión de `app`** en DBeaver (o
`psql -U app -h localhost -d monitoreo_ambiental -f database/esquema.sql`),
ejecutar [`database/esquema.sql`](../../database/esquema.sql). Como `app`
es dueño de la base, las tablas quedan a su nombre y RLS no interfiere
en desarrollo local.

## 6. Variables de entorno

Los datos de conexión están en [`.env.example`](../../.env.example)
(`PG_HOST`, `PG_PORT`, `PG_DB`, `PG_USER`, `PG_PASS`). Copiar el archivo
a `backend/.env` y completar `PG_PASS` ahí — `.env` está en
`.gitignore`, la contraseña real nunca se versiona.

## Archivos

| Archivo | Para qué sirve |
|---|---|
| `01_instalar_db.sql` | crea base + usuario + permisos (se superusuario) |
| `../../database/esquema.sql` | crea las tablas y vistas (se usuario `app`) |
| `../../.env.example` | plantilla de conexión (sin credenciales) |
