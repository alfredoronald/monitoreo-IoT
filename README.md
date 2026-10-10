# Monitoreo IoT — Calidad del aire en el aula

Monorepo del prototipo de monitoreo ambiental: un **ESP32** lee los
sensores, publica por **MQTT** a un broker **Mosquitto** local, un
backend **Node.js + Express** guarda las lecturas en **PostgreSQL** y un
dashboard **React + Recharts** las grafica casi en tiempo real.

```
ESP32 (DHT22 + MQ-135) ──MQTT──> Mosquitto ──> Backend (Express) ──> PostgreSQL
                                                                  │
                              Dashboard (React) <──REST /api/lecturas
```

## Tecnologías

| Capa | Tecnología |
|---|---|
| Microcontrolador | ESP32 (WiFi + Bluetooth + convertidores ADC1/ADC2) |
| Firmware | C++ con **PlatformIO** (gestiona librerías y compilación mejor que el IDE clásico de Arduino) |
| Sensores | DHT22 (temperatura y humedad) · MQ-135 (estimación de CO2 y calidad del aire) |
| Broker MQTT | Mosquitto (Eclipse), local, puerto 1883 |
| Backend | Node.js + **Express** (mqtt.js, pg) |
| Base de datos | PostgreSQL local, administrada con DBeaver |
| Frontend | **React** + **Recharts** |
| Infraestructura | Instalaciones locales — **sin Docker** (sección 6.3 del informe) |

## Estructura del monorepo

```
monitoreo-IoT/
├── backend/      # Express + suscriptor MQTT + endpoints
├── frontend/     # Dashboard React + Recharts
├── firmware/     # Código del ESP32 (PlatformIO)
│   └── docs/     # pinout y decisiones de hardware
├── database/     # script SQL del esquema (tabla lecturas)
├── infra/        # configuración de Mosquitto y PostgreSQL (solo archivos)
└── docs/         # informe, diagramas y evidencias
```

## Ramas

| Rama | Propósito |
|---|---|
| `main` | Código estable y probado, listo para la demostración final |
| `develop` | Rama de integración que recibe PR de las ramas de área, no de tareas individuales |
| `feature/firmware-esp32` | Lecturas de sensores y protocolo PubSubClient |
| `feature/backend-mqtt-api` | Broker, backend, PostgreSQL y sus tareas y evidencias |
| `feature/frontend-dashboard` | Componentes React, gráficos Recharts y hojas de estilos |
| `feature/repo-guidelines` | Normas, tablero y documentación general del repositorio |

## Cómo levantar cada parte

### 1. Requisitos y preparación (Windows / PowerShell)

Este recorrido usa **Windows / PowerShell, sin Docker**. No hace falta un ESP32;
los bloques indican desde qué carpeta y consola ejecutar los comandos.

Instalar Git y Node.js LTS (22 o 24) con `winget`, disponible mediante
**Instalador de aplicación** de Microsoft Store:

```powershell
winget install --id Git.Git -e
winget install --id OpenJS.NodeJS.LTS -e
```

Cerrar y abrir PowerShell para cargar el PATH actualizado. Instalar pnpm 10
(compatible con `backend/pnpm-lock.yaml`) y comprobar las herramientas:

```powershell
npm install --global pnpm@10
git --version
node --version
pnpm --version
```

Desde la carpeta donde se guardará el proyecto:

```powershell
git clone https://github.com/alfredoronald/monitoreo-IoT.git
Set-Location monitoreo-IoT
git switch feature/backend-mqtt-api
git pull --ff-only origin feature/backend-mqtt-api
```

Si ya existe una copia, actualizarla desde su raíz con los dos últimos comandos,
conservando el trabajo local. Para contribuir, seguir [CONTRIBUTING.md](docs/CONTRIBUTING.md)
y crear una rama de tarea desde esa base actualizada.

### 2. Preparar PostgreSQL

El backend requiere PostgreSQL para abrir el puerto HTTP. Instalarlo con el
[instalador para Windows](https://www.postgresql.org/download/windows/), conservar
el puerto **5432** y definir la contraseña de `postgres`. En `services.msc`, iniciar
`postgresql-x64-<versión>`. Más detalles y DBeaver: [guía de PostgreSQL](infra/postgres/README.md).

Desde la **raíz del repositorio**, agregar `psql` al PATH de esta consola
(cambiar `18` si se instaló otra versión) y crear la base y el rol `app`:

```powershell
$env:Path += ";C:\Program Files\PostgreSQL\18\bin"
psql --version
psql -U postgres -h localhost -d postgres -f infra/postgres/01_instalar_db.sql
psql -U postgres -h localhost -d postgres
```

Introducir la contraseña de `postgres`. En `psql`, asignar la de `app` y salir:

```text
\password app
\q
```

`\password` solicita y confirma la contraseña. De nuevo en PowerShell, cargar
el esquema **como `app`**, para que las tablas sean suyas:

```powershell
psql -U app -h localhost -d monitoreo_ambiental -f database/esquema.sql
psql -U app -h localhost -d monitoreo_ambiental -c "SELECT current_user, current_database();"
```

La consulta debe devolver `app` y `monitoreo_ambiental` tras introducir la contraseña
de `app`. Si la base ya existía, `database already exists` es esperado; conservarla.

### 3. Instalar y configurar Mosquitto 2.x

```powershell
winget install --id EclipseFoundation.Mosquitto -e
& "C:\Program Files\mosquitto\mosquitto.exe" -h
```

La ayuda debe mostrar la versión 2.x. Se usa la ruta completa porque Mosquitto
puede no estar en el PATH; reemplazarla si se eligió otra carpeta de instalación.

El servicio de Windows usa su propia configuración y puede ocupar el puerto
1883. En una **PowerShell como administrador**, detenerlo y dejarlo en manual:

```powershell
if (Get-Service -Name mosquitto -ErrorAction SilentlyContinue) {
    Stop-Service -Name mosquitto
    Set-Service -Name mosquitto -StartupType Manual
}
```

Volver a la consola normal, en la **raíz del repositorio**, y crear los usuarios:

```powershell
if (-not (Test-Path -LiteralPath infra/mosquitto/passwd)) {
    & "C:\Program Files\mosquitto\mosquitto_passwd.exe" -c infra/mosquitto/passwd esp32
} else {
    & "C:\Program Files\mosquitto\mosquitto_passwd.exe" infra/mosquitto/passwd esp32
}
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" infra/mosquitto/passwd backend
New-Item -ItemType Directory -Force -Path infra/mosquitto/data | Out-Null
```

Cada comando solicita y confirma una contraseña: guardar la de `esp32` para
publicar y la de `backend` para `MQTT_PASS`. **`-c` sobrescribe el archivo completo**:
usarlo solo al crearlo. Sin `-c`, agrega o actualiza un usuario. `passwd.example`
solo ilustra el formato; cada integrante genera su `passwd`, ignorado por Git.

La configuración [mosquitto.conf](infra/mosquitto/mosquitto.conf) escucha en **1883**,
deshabilita conexiones anónimas y usa [acl](infra/mosquitto/acl):

| Usuario MQTT | Permiso |
|---|---|
| `esp32` | Publicar en `ambiente/#` |
| `backend` | Leer y suscribirse a `ambiente/#` |

### 4. Configurar el entorno del backend

Desde la **raíz**, copiar la plantilla solo si aún no existe el archivo local:

```powershell
if (-not (Test-Path -LiteralPath backend/.env)) {
    Copy-Item -LiteralPath .env.example -Destination backend/.env
}
notepad backend/.env
```

Completar y guardar las variables de [`.env.example`](.env.example):

| Variable | Valor local / propósito | Requisito |
|---|---|---|
| `PG_HOST` | `localhost`, servidor PostgreSQL | Obligatoria |
| `PG_PORT` | `5432`, puerto PostgreSQL | Predeterminado: `5432` |
| `PG_DB` | `monitoreo_ambiental` | Obligatoria |
| `PG_USER` | `app`, rol dueño de las tablas | Obligatoria |
| `PG_PASS` | Contraseña del rol PostgreSQL `app` | Obligatoria |
| `MQTT_URL` | `mqtt://localhost:1883` | Obligatoria |
| `MQTT_USER` | `backend`, usuario creado con `mosquitto_passwd` | Obligatoria |
| `MQTT_PASS` | Contraseña MQTT del usuario `backend` | Obligatoria |
| `API_PORT` | `3000`, puerto HTTP | Predeterminado: `3000` |
| `CORS_ORIGIN` | `http://localhost:5173`, origen del frontend | Tiene ese valor predeterminado |
| `API_KEY` | Vacía; prevista para el Sprint 3 | No condiciona el arranque actual |

`PG_PASS` y `MQTT_PASS` son de servicios distintos. Guardarlas solo en `backend/.env`;
si contienen `#`, envolver el valor en comillas dobles para evitar comentarios
de dotenv. El código usa `PG_*`; no lee las variables `SUPABASE_*` de la plantilla.

Instalar las dependencias desde `backend/`, respetando el archivo de bloqueo:

```powershell
Set-Location backend
pnpm install --frozen-lockfile
```

### 5. Levantar y comprobar los servicios

Mantener PostgreSQL iniciado y abrir **tres terminales PowerShell** en la raíz
del repositorio (en el explorador, abrir la carpeta y elegir «Abrir en Terminal»).

**Terminal 1 — broker:** iniciar con la configuración del repositorio, desde
la raíz; las rutas del archivo son relativas a esa carpeta:

```powershell
& "C:\Program Files\mosquitto\mosquitto.exe" -c infra/mosquitto/mosquitto.conf -v
```

Debe informar que abre el puerto **1883**. Mantener esta terminal abierta.

**Terminal 2 — backend:**

```powershell
Set-Location backend
pnpm dev
```

El modo de desarrollo reinicia al cambiar código. Debe mostrar (en cualquier orden):

```text
Conexión PostgreSQL establecida en localhost:5432/monitoreo_ambiental
Servidor de monitoreo ambiental disponible en http://localhost:3000
[MQTT] Conectado al broker
[MQTT] Suscrito a ambiente/#
```

**Terminal 3 — comprobación HTTP y publicación MQTT:**

```powershell
Invoke-RestMethod -Uri http://localhost:3000/ | ConvertTo-Json -Compress
```

Debe responder HTTP 200 con:

```json
{"status":"ok","message":"Servidor de monitoreo ambiental activo"}
```

Esta ruta comprueba Express; las rutas `/api/lecturas`, `/api/lecturas/ultima`
y `/api/salud` se implementarán en sus tareas respectivas.

Publicar con la contraseña de **`esp32`**, solicitándola sin mostrarla al escribir:

```powershell
$mqttSecret = Read-Host 'Contraseña MQTT de esp32' -AsSecureString
$mqttPassword = [System.Net.NetworkCredential]::new('', $mqttSecret).Password
try {
    & "C:\Program Files\mosquitto\mosquitto_pub.exe" -h localhost -p 1883 -u esp32 -P "$mqttPassword" -t ambiente/temperatura -m '25.0'
    & "C:\Program Files\mosquitto\mosquitto_pub.exe" -h localhost -p 1883 -u esp32 -P "$mqttPassword" -t ambiente/humedad -m '55'
    & "C:\Program Files\mosquitto\mosquitto_pub.exe" -h localhost -p 1883 -u esp32 -P "$mqttPassword" -t ambiente/co2 -m '700'
} finally {
    Remove-Variable mqttPassword, mqttSecret
}
```

En la **terminal 2** deben aparecer:

```text
[MQTT] ambiente/temperatura: 25.0
[MQTT] ambiente/humedad: 55
[MQTT] ambiente/co2: 700
```

Para probar reconexión, detener **solo el broker** con `Ctrl+C` en la terminal 1.
El backend debe seguir activo e intentar reconectar cada **2 segundos**.
Repetir el comando de arranque del broker en esa terminal: el backend debe
volver a conectar y suscribirse sin reiniciarlo. Repetir el bloque de publicación
en la terminal 3 y comprobar de nuevo la recepción.

Finalizar con `Ctrl+C` en el backend y el broker. En los próximos arranques,
comprobar PostgreSQL e iniciar ambos sin recrear usuarios ni copiar `.env`.

### 6. Comprobar tipos y ejecutar la compilación

Con el broker y PostgreSQL activos, detener `pnpm dev` para liberar el puerto
HTTP y ejecutar **dentro de `backend/`**:

```powershell
pnpm typecheck
pnpm build
pnpm start
```

`typecheck` y `build` deben terminar sin errores. `build` genera `dist/`; `start`
ejecuta `dist/index.js`. Repetir las comprobaciones del paso 5 y terminar con `Ctrl+C`.

### 7. Problemas frecuentes

| Síntoma | Solución |
|---|---|
| `node`, `pnpm` o `psql` no se reconoce | Abrir una consola nueva tras instalar. Para `psql`, agregar la carpeta `bin` de la versión instalada al PATH como en el paso 2. |
| PowerShell bloquea `npm.ps1` o `pnpm.ps1` | Usar `npm.cmd` o `pnpm.cmd` en los mismos comandos. |
| Mosquitto no se encuentra | Usar la ruta completa entre comillas y el operador `&`; comprobar su carpeta de instalación. |
| Mosquitto no puede abrir `passwd`, `acl` o guardar persistencia | Ejecutarlo desde la raíz, crear los usuarios y la carpeta `infra/mosquitto/data`, y comprobar permisos sobre ella. |
| El puerto 1883 ya está en uso | Detener el servicio Mosquitto como administrador o cerrar otra instancia del broker antes de iniciar la del repositorio. |
| MQTT informa `Not authorized` o no llegan mensajes | Revisar que `MQTT_PASS` corresponde a `backend` y la publicación usa `esp32`. Confirmar los nombres del ACL y reiniciar el broker tras cambiar usuarios. |
| Faltan variables o falla la conexión PostgreSQL | Completar `PG_*` en `backend/.env`, comprobar el servicio y repetir la consulta del paso 2 con el rol `app`. Reiniciar `pnpm dev` tras editar `.env`. |
| `EADDRINUSE` en el backend | Detener el otro backend antes de iniciar `pnpm start`, o cambiar `API_PORT` y usar ese puerto en la consulta HTTP. |

### 8. Revisión y evidencia de #23

La evidencia de #23 es este README en GitHub. Otra persona debe seguir los pasos
1–6 desde una instalación nueva y confirmar en la PR el arranque, HTTP, recepción
MQTT y reconexión sin ayuda. Las evidencias no deben mostrar contraseñas ni `.env`.

### Frontend y firmware

- **Frontend:** desde `frontend/`, ejecutar `pnpm install` y `pnpm dev` (puerto 5173).
- **Firmware:** cuando esté disponible la plantilla `config.example.h`, copiarla
  a `config.h`, completar WiFi y MQTT y ejecutar `pio run -t upload` desde `firmware/`.

## Contrato de datos

### Topics MQTT — suscripción `ambiente/#`

| Topic | Contenido |
|---|---|
| `ambiente/temperatura` | Temperatura en °C (DHT22) |
| `ambiente/humedad` | Humedad relativa en % (DHT22) |
| `ambiente/co2` | CO2 **estimado** en ppm (MQ-135) |
| `ambiente/status` | `online` / `offline` (retained + LWT del ESP32) |

### API REST

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/lecturas?variable=&desde=&hasta=&limite=` | Lecturas por rango de fechas |
| GET | `/api/lecturas/ultima` | Último valor por variable |
| GET | `/api/salud` | Estado de la base y del broker |

### Base de datos

```sql
lecturas (id, fecha_hora, tipo_variable, valor)
  -- tipo_variable ∈ { temperatura, humedad, co2 }
  -- índices sobre fecha_hora y (tipo_variable, fecha_hora)
```

## Notas de hardware

- El **MQ-135 va en un pin de ADC1**: el ADC2 lo comparte con el WiFi, por
  eso se usa el convertidor 1 (Espressif Systems, s.f.).
- El calefactor del MQ-135 trabaja a 5 V: su salida pasa por un **divisor
  de voltaje** antes de llegar al ADC de 3,3 V.
- El DHT22 no se puede leer más de una vez cada 2 segundos (hoja de datos);
  nosotros publicamos cada 10–30 s, no nos afecta.
- El MQ-135 necesita **más de 24 h de precalentamiento** la primera vez y
  una calibración de Ro en aire limpio; no es un medidor exacto de CO2,
  sino una forma de estimar el nivel y el estado general del aire.
