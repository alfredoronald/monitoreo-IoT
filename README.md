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

Cada comando solicita y confirma una contraseña: usar claves aleatorias distintas
de **al menos 12 caracteres** (se recomiendan 20 o más). Guardar la de `esp32` para
publicar y la de `backend` para `MQTT_PASS`. **`-c` sobrescribe el archivo completo**:
usarlo solo al crearlo. Sin `-c`, agrega o actualiza un usuario. `passwd.example`
solo ilustra el formato; cada integrante genera su `passwd`, ignorado por Git.

La configuración [mosquitto.conf](infra/mosquitto/mosquitto.conf) escucha en **1883**,
deshabilita conexiones anónimas y usa [acl](infra/mosquitto/acl):

| Usuario MQTT | Permiso |
|---|---|
| `esp32` | Publicar en `ambiente/#` |
| `backend` | Leer y suscribirse a `ambiente/#` |

#### Rotar las contraseñas MQTT (#133)

Desde la raíz, con los clientes detenidos, actualizar los dos usuarios existentes
sin `-c`. Los comandos solicitan las contraseñas sin mostrarlas en pantalla:

```powershell
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" -H sha512-pbkdf2 infra/mosquitto/passwd esp32
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" -H sha512-pbkdf2 infra/mosquitto/passwd backend
```

`sha512-pbkdf2` permite usar el archivo tanto en Mosquitto 2.0 como en 2.1.
Crear dos claves nuevas distintas y de al menos 12 caracteres. A continuación:

1. En `backend/.env`, dejar `MQTT_USER=backend` y actualizar `MQTT_PASS` con
   la clave nueva de `backend`.
2. En `firmware/include/config.h`, dejar `mqtt_user` como `esp32` y actualizar
   `mqtt_password` con la clave nueva de `esp32`. Conservar la configuración WiFi.
3. Revisar localmente los nombres de usuario de `passwd`: deben quedar únicamente
   `esp32` y `backend`. Si existe otro usuario, eliminarlo con `mosquitto_passwd -D`
   indicando el archivo y el nombre. No mostrar ni adjuntar el archivo.
4. Reiniciar Mosquitto con la configuración del repositorio y reiniciar el backend.
   El reinicio del broker cierra también las sesiones que usaban claves anteriores.
5. Compilar y cargar el firmware al ESP32. Cambiar `config.h` en la computadora
   **no actualiza el dispositivo**: la contraseña anterior sigue en su memoria
   hasta cargar el firmware nuevo.

Las tres rutas privadas están ignoradas por Git, incluido el archivo de hashes
`passwd`. Si se introduce `firmware/include/secrets.h`, agregarlo a `.gitignore`
antes de guardar credenciales en él. No adjuntar binarios del firmware: también
contienen la configuración privada y ya quedan excluidos dentro de `.pio/`.

El ACL deniega por defecto toda operación sin permiso. No agregar `topic deny #`,
ya que una denegación explícita puede bloquear también los permisos legítimos.
Un filtro de suscripción aceptado (`SUBACK`) no demuestra permiso de lectura:
Mosquitto también comprueba el ACL al entregar cada mensaje.

Para verificar la rotación, publicar como `esp32` y recibir como `backend` en
`ambiente/#`; comprobar también que `esp32` no recibe esos mensajes. Las
publicaciones del backend y las de cualquiera fuera de `ambiente/#` deben ser
rechazadas. Para comprobar publicaciones denegadas, usar MQTT 5 y QoS 1
(`mosquitto_pub -V mqttv5 -q 1`): el broker devuelve «Not authorized» (135).
Una publicación QoS 0 que termina sin error no demuestra autorización.
Comprobar además el rechazo de acceso anónimo, usuario desconocido y claves
incorrectas o anteriores. No escribir contraseñas literales en los comandos.

Los resultados locales y la comprobación física pendiente están en la
[evidencia de #133](docs/evidencias/133-acl-mosquitto.md).

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

#### CORS y errores de la API (#77)

`CORS_ORIGIN` admite un único origen exacto (protocolo, host y puerto),
sin ruta ni barra final, por ejemplo `http://localhost:5173`. No usar `*`.
`http://127.0.0.1:5173` y `http://localhost:5174` son orígenes distintos.
Las peticiones con otro encabezado `Origin` responden 403 con
`{"mensaje":"Origen no permitido por CORS."}`, sin habilitar su lectura
desde ese origen. El preflight OPTIONS del dashboard admite GET y PATCH
con Content-Type. No se habilitan credenciales de navegador.
Los clientes sin `Origin`, como curl, siguen funcionando: CORS no reemplaza
la autenticación prevista para Sprint 3.

Los fallos inesperados de las rutas pasan al manejador central y responden
500 con `{"mensaje":"No se pudo completar la solicitud."}`. Los errores
de JSON responden 400; los cuerpos que superan el límite del parser, 413.
Las respuestas no exponen detalles internos. La validación de parámetros
y los errores esperados de cada ruta mantienen sus códigos y mensajes.

Para sacar la evidencia sin crear archivos adicionales:

1. Iniciar backend y frontend; abrir el dashboard en el origen configurado.
2. En las herramientas de desarrollo del navegador, abrir la consola y ejecutar:

   ```javascript
   fetch('http://localhost:3000/api/lecturas/ultima')
     .then(async (r) => console.log('HTTP', r.status, await r.json()));
   ```

3. En Network/Red, seleccionar la petición `ultima` y capturar la URL,
   código 200 y encabezados `Origin` de la solicitud y
   `Access-Control-Allow-Origin` de la respuesta. Deben coincidir con el
   origen del dashboard. No abrir directamente la URL de la API para esta
   evidencia: se necesita una petición hecha desde la página del dashboard.
4. Adjuntar la captura al PR. Las pruebas de orígenes rechazados, preflight
   y errores internos se ejecutan con `npm test` desde `backend/`.

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

Esta ruta comprueba Express. Las consultas de lecturas se explican en
[API REST](#api-rest). `/api/salud` sigue pendiente de implementación.

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

### Log de mensajes MQTT (#21)

El backend registra en `backend/logs/mqtt-messages.jsonl` cada mensaje
recibido (`received`) y cada descarte (`discarded`). El archivo se crea
al recibir el primer mensaje y se conserva entre ejecuciones: cada línea
contiene un JSON con `timestamp`, `event`, `topic` y `content`.
Los descartes incluyen también `reason`, tomado del validador de #20.
Un mensaje inválido tiene un registro de recepción y otro de descarte.
Un fallo al insertar una lectura válida es un error de base de datos,
no un descarte del validador.

Los mismos registros se muestran en consola. Los saltos de línea del
contenido quedan escapados, para que un descarte ocupe una única línea.
Se ocultan los valores secretos configurados en el entorno y las
contraseñas identificadas en mensajes o URLs; no se vuelca `.env`.
`backend/logs/` está ignorado por Git. Si no se puede escribir el archivo,
se avisa en consola y se continúa procesando los mensajes.

Para obtener la evidencia, levantar broker y backend como se indica arriba.
Publicar como `esp32` un valor válido en `ambiente/temperatura`, un valor
no numérico en `ambiente/humedad`, un mensaje en `ambiente/status` y otro
en `ambiente/presion`. Revisar el archivo desde la raíz en Git Bash:

```bash
cat backend/logs/mqtt-messages.jsonl
```

Cada descarte debe mostrar el topic, contenido y motivo en la misma línea.
Adjuntar el archivo de log a la PR como evidencia después de revisar que
solo contiene los mensajes de prueba y ningún dato sensible.
Las pruebas automatizadas usan archivos temporales fuera del repositorio:
desde `backend/`, ejecutar `npm test`.

### Prueba de carga de 100 mensajes (#22)

La prueba ejecuta `mosquitto_pub` 100 veces en un bucle, sin pausa entre
envíos. Cada ejecución abre una conexión MQTT. Usa QoS **0** tanto en la
publicación como en la suscripción actual del backend: no hay confirmación
MQTT de entrega. Un comando que finaliza correctamente cuenta como enviado;
la recepción se verifica por separado en el log.

1. Levantar PostgreSQL, Mosquitto y **una sola instancia del backend**.
   Detener el ESP32 y otros publicadores durante la medición.
2. Comprobar que el backend muestra `Suscrito a ambiente/#` y que existe
   `backend/logs/mqtt-messages.jsonl` (enviar un mensaje previo si falta).
3. En otra terminal **Git Bash**, desde `backend/`, ejecutar:

   ```bash
   read -s -p "Contraseña MQTT de esp32: " MQTT_PUBLISH_PASS
   export MQTT_PUBLISH_PASS
   npm run test:load
   unset MQTT_PUBLISH_PASS
   ```

El script toma la conexión PostgreSQL y la URL del broker de `backend/.env`.
La contraseña de publicación corresponde a `esp32`, no al usuario `backend`.
No se imprime ni se guarda en el repositorio. En Windows se busca
`C:\Program Files\mosquitto\mosquitto_pub.exe`; para otra instalación,
definir `MOSQUITTO_PUB_PATH` en esa terminal con la ruta del ejecutable.

Se publican 100 valores sintéticos distintos en `ambiente/co2`, dentro de
un rango que no contiene lecturas previas. **No son mediciones reales**:
quedan guardados en la base local como datos de prueba. El script cuenta
solo registros nuevos del log y filas nuevas de ese rango en PostgreSQL;
no borra el historial. Espera hasta 15 segundos después del último envío
para permitir que terminen las inserciones.

La terminal imprime una tabla Markdown con enviados, recibidos, guardados,
QoS y pérdidas, además de duplicados y el rango usado. Si una publicación
falla, informa que la prueba quedó incompleta; no presenta 100 envíos como
si hubieran ocurrido. Para la evidencia, copiar **la tabla real que imprime**
en el PR y adjuntar una captura de esa terminal. No hace falta crear un
documento adicional en el proyecto. Conservar también el log para revisar
el detalle. Si hay pérdidas, anotarlas aunque el resultado no sea 100/100/100.

### Frontend y firmware

- **Frontend:** desde `frontend/`, ejecutar `pnpm install` y `pnpm dev` (puerto 5173).
- **Firmware:** en VS Code con PlatformIO IDE, abrir PlatformIO Home,
   elegir **Open Project** y seleccionar la carpeta `firmware/`, donde
   está `platformio.ini`. Abrir una terminal de PlatformIO en esa carpeta.

   Crear la configuración privada copiando la plantilla (PowerShell):

   ```powershell
   Copy-Item include/config.example.h include/config.h
   ```

   Completar en `include/config.h` el SSID, la contraseña WiFi y el host,
   puerto, usuario y contraseña del broker. Este archivo está excluido
   por `.gitignore`. Completar también el identificador MQTT único y los
   intervalos/timeouts de la plantilla. El firmware conecta y reconecta WiFi
   y MQTT mientras el DHT22 continúa adquiriendo muestras; consultar el
   [procedimiento T-1.6](firmware/docs/wifi-mqtt-reconnect.md).

   Compilar desde `firmware/`:

   ```bash
   pio run
   ```

   Conectar el ESP32 por USB y cargar el firmware:

   ```bash
   pio run -t upload
   ```

   Abrir el monitor serie a 115200 baudios:

   ```bash
   pio device monitor --baud 115200
   ```

   Con el monitor abierto, pulsar **EN/RESET** en el ESP32 para observar
   el mensaje `Firmware ESP32 iniciado`, la IP WiFi y la conexión MQTT.
   Salir del monitor con `Ctrl+C`.

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
| GET | `/api/lecturas?variable=&desde=&hasta=` | Lecturas de una variable por rango de fechas |
| GET | `/api/lecturas/ultima` | Último valor por variable |
| GET | `/api/salud` | Estado de la base y del broker (pendiente) |

Base local: `http://localhost:3000/api`. Las peticiones GET no llevan cuerpo.
Las respuestas usan `Content-Type: application/json`. En Sprint 2 no se
exige `API_KEY`. El contrato completo está en [docs/api.md](docs/api.md).
Los valores de los ejemplos son ilustrativos y dependen de las lecturas
que existan en la base; no son resultados de una ejecución real.

#### GET /api/lecturas: histórico por variable y fechas

| Parámetro | Obligatorio | Formato |
|---|---|---|
| `variable` | Sí | `temperatura`, `humedad` o `co2` |
| `desde` | Sí | Fecha ISO 8601 con hora, segundos y zona horaria |
| `hasta` | Sí | Fecha ISO 8601 con hora, segundos y zona horaria; igual o posterior a `desde` |

Ejemplos de fecha: `2026-10-15T12:00:00Z` o `2026-10-15T08:00:00-04:00`.
Se admiten fracciones de segundo. Los extremos del rango son inclusivos.
Las lecturas se ordenan por `fecha_hora ASC, id ASC`. No se admite `limite`
ni se truncan silenciosamente los resultados.

Llamada desde Git Bash:

```bash
curl -i "http://localhost:3000/api/lecturas?variable=temperatura&desde=2026-10-15T12:00:00Z&hasta=2026-10-15T13:00:00Z"
```

Ejemplo de respuesta **HTTP 200**:

```json
[
  { "id": "101", "tipo_variable": "temperatura", "valor": 24.5, "fecha_hora": "2026-10-15T12:00:00.000Z" },
  { "id": "104", "tipo_variable": "temperatura", "valor": 24.7, "fecha_hora": "2026-10-15T12:00:15.000Z" }
]
```

`id` es una cadena para conservar la precisión de BIGINT; `valor` es un
número JSON. La unidad depende de la variable: °C, % HR o ppm estimados de
CO₂. Las fechas de salida están en UTC. Si no hay coincidencias, devuelve
**HTTP 200**, no 404, con una lista vacía:

```json
[]
```

#### GET /api/lecturas/ultima: últimos valores disponibles

Devuelve la lectura más reciente de cada variable mediante la vista
`v_ultima_lectura`. No admite parámetros de consulta ni cuerpo.

```bash
curl -i "http://localhost:3000/api/lecturas/ultima"
```

Ejemplo de respuesta **HTTP 200**:

```json
[
  { "tipo_variable": "co2", "valor": 900, "fecha_hora": "2026-10-15T12:00:15.000Z" },
  { "tipo_variable": "humedad", "valor": 55, "fecha_hora": "2026-10-15T12:00:14.000Z" },
  { "tipo_variable": "temperatura", "valor": 24.7, "fecha_hora": "2026-10-15T12:00:15.000Z" }
]
```

El resultado está ordenado por `tipo_variable ASC`, con hasta tres
elementos y sin campo `id`. Si una variable no tiene lecturas, se omite;
si no existe ninguna lectura, responde **HTTP 200** con `[]`. La fecha
permite distinguir una lectura antigua de un valor reciente: no se
sustituyen datos ausentes por cero.

#### Errores HTTP 400

Un 400 indica que la solicitud debe corregirse. La respuesta contiene un
campo `mensaje` en español. Reglas de validación acordadas en el contrato:

| Caso | Ejemplo incorrecto | Motivo |
|---|---|---|
| Falta un parámetro obligatorio | Omitir `hasta` | El histórico necesita variable y ambas fechas |
| Parámetro vacío o repetido | `variable=` o dos `desde` | Cada parámetro debe ser escalar, no vacío y único |
| Variable desconocida | `variable=ruido` | Solo temperatura, humedad y co2 |
| Fecha mal escrita o imposible | `desde=ayer` o 30 de febrero | Se requiere una fecha ISO 8601 válida |
| Fecha sin zona horaria | `2026-10-15T12:00:00` | Incluir `Z` o desplazamiento horario |
| Rango invertido | `desde` posterior a `hasta` | El inicio no puede superar el final |
| Parámetro adicional | `limite=10` | Solo se admiten los tres filtros documentados |
| Query en últimas lecturas | `/api/lecturas/ultima?variable=co2` | Esta ruta no admite filtros |
| Cuerpo en un GET | Enviar un objeto JSON | Estas consultas no admiten cuerpo |

Ejemplo para provocar un **400** sin depender de datos en PostgreSQL:

```bash
curl -i "http://localhost:3000/api/lecturas/ultima?variable=co2"
```

Respuesta **HTTP 400 Bad Request**:

```json
{ "mensaje": "Esta ruta no admite parámetros de consulta." }
```

Ejemplo con una variable desconocida:

```bash
curl -i "http://localhost:3000/api/lecturas?variable=ruido&desde=2026-10-15T12:00:00Z&hasta=2026-10-15T13:00:00Z"
```

Respuesta **HTTP 400 Bad Request**:

```json
{ "mensaje": "variable debe ser temperatura, humedad o co2." }
```

La validación ocurre antes de consultar PostgreSQL. Por ejemplo, omitir
`hasta` devuelve `{"mensaje":"El parámetro hasta es obligatorio."}` y un
rango invertido devuelve `{"mensaje":"desde no puede ser posterior a hasta."}`.

Un fallo inesperado del servidor no es un 400: responde **500** con
`{"mensaje":"No se pudo completar la solicitud."}`, sin detalles internos.

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
