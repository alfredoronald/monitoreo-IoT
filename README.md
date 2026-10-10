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

> Próximos pasos: los archivos de configuración y código se agregan en la
> task de implementación.

1. **Base de datos** — crear la base `monitoreo_ambiental` y el usuario de
   la app; después ejecutar el script de `database/` con DBeaver o `psql`.
2. **Broker MQTT** — instalar Mosquitto 2.x y levantarlo con la
   configuración del repositorio:

   ```bash
   winget install --id EclipseFoundation.Mosquitto -e
   ```

   El instalador no agrega `C:\Program Files\mosquitto` al `PATH`. En la
   consola abierta se recarga sin reiniciar (PowerShell):

   ```powershell
   $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
   ```

   El servicio automático de Windows arranca con la configuración por
   defecto y ocuparía el puerto 1883; se detiene y se deja en manual
   (PowerShell):

   ```powershell
   Stop-Service -Name mosquitto
   Set-Service -Name mosquitto -StartupType Manual
   ```

   Crear los usuarios locales (`infra/mosquitto/passwd` **no** se
   versiona, cada quien usa sus propias credenciales; el repo trae
   `infra/mosquitto/passwd.example` con hashes de ejemplo del formato):

   ```bash
   mosquitto_passwd -c -b infra/mosquitto/passwd esp32 <contraseña>
   mosquitto_passwd -b infra/mosquitto/passwd backend <contraseña>
   ```

   Levantar el broker **desde la raíz del repo** (las rutas de
   `mosquitto.conf` son relativas a la raíz):

   ```bash
   mosquitto -c infra/mosquitto/mosquitto.conf -v
   ```

   Sin usuarios no hay conexión: desde la versión 2.0, al definir un
   `listener` el broker deja de aceptar clientes anónimos, y con
   `allow_anonymous false` más `password_file` y `acl_file` cada cliente
   necesita sus credenciales y solo puede tocar los topics del ACL
   (`esp32` publica en `ambiente/#`, `backend` solo lee).
3. **Backend** — preparar el entorno desde la raíz del repositorio (PowerShell):

   ```powershell
   Copy-Item .env.example backend/.env
   cd backend
   pnpm install
   pnpm dev
   ```

   Si ya existe `backend/.env`, conservar sus valores. La plantilla
   `.env.example` está versionada; el archivo `.env` local está ignorado por Git.
   Completar `PG_HOST`, `PG_PORT`, `PG_DB`, `PG_USER` y `PG_PASS`. El
   backend verifica la conexión a PostgreSQL antes de escuchar en el puerto
   HTTP y muestra un error claro si la base no está disponible.
   Completar `MQTT_URL`, `MQTT_USER=backend` y `MQTT_PASS` con las credenciales
   locales del broker. Las tres variables MQTT son obligatorias.
   Si PostgreSQL está disponible y Mosquitto está detenido, el backend
   permanece activo e intenta reconectar cada 2 segundos.
   `API_KEY` puede quedar vacía; se utilizará en el Sprint 3.
   La plantilla define `API_PORT=3000` y `CORS_ORIGIN=http://localhost:5173`.

   El modo de desarrollo reinicia el servidor al cambiar el código.
   Abrir `http://localhost:3000/` debe devolver HTTP 200:

   ```json
   {"status":"ok","message":"Servidor de monitoreo ambiental activo"}
   ```

   Esta ruta comprueba el arranque de Express. Los endpoints `/api/lecturas`,
   `/api/lecturas/ultima` y `/api/salud` se implementarán en sus tareas respectivas.

   Para comprobar tipos, compilar y ejecutar, dentro de `backend/`:

   ```powershell
   pnpm typecheck
   pnpm build
   pnpm start
   ```

   Detener `pnpm dev` con `Ctrl+C` antes de ejecutar `pnpm start` para liberar
   el puerto 3000. `build` genera `dist/` y `start` ejecuta `dist/index.js`
   directamente en Node. `tsconfig.json` hereda de `../tsconfig.base.json`
   y usa `module` y `moduleResolution` en `NodeNext`; los imports locales
   usan extensión `.js` para los archivos compilados.
   `dist/` y `node_modules/` están ignorados por Git.

   Desde la raíz, comprobar los archivos antes del commit:

   ```powershell
   git check-ignore backend/.env
   git ls-files .env.example
   git status --short
   ```

   Los dos primeros comandos deben mostrar `backend/.env` y `.env.example`,
   respectivamente. La evidencia de la issue #18 será el commit publicado en GitHub.

   **Comprobar el suscriptor MQTT (#19):** con Mosquitto activo y `pnpm dev`
   ejecutándose, la consola del backend debe mostrar `[MQTT] Conectado al broker`
   y `[MQTT] Suscrito a ambiente/#`. En otra terminal Git Bash desde la raíz:

   ```bash
   read -r -s -p 'Contraseña MQTT de esp32: ' MQTT_TEST_PASS
   printf '\n'
   mosquitto_pub -h localhost -p 1883 -u esp32 -P "$MQTT_TEST_PASS" -t ambiente/temperatura -m '25.0'
   mosquitto_pub -h localhost -p 1883 -u esp32 -P "$MQTT_TEST_PASS" -t ambiente/humedad -m '55'
   mosquitto_pub -h localhost -p 1883 -u esp32 -P "$MQTT_TEST_PASS" -t ambiente/co2 -m '700'
   unset MQTT_TEST_PASS
   ```

   Si Mosquitto no está en el PATH de Git Bash, usar
   `"/c/Program Files/mosquitto/mosquitto_pub.exe"` en lugar de `mosquitto_pub`.
   Cada mensaje debe aparecer con su topic y contenido, por ejemplo
   `[MQTT] ambiente/temperatura: 25.0`. Para comprobar la reconexión, detener
   solo el broker con `Ctrl+C` y volver a iniciarlo desde la raíz con
   `mosquitto -c infra/mosquitto/mosquitto.conf -v`. El backend debe reconectar
   y suscribirse sin reiniciarlo; publicar otro mensaje para confirmar
   que vuelve a recibir. La evidencia de #19 es una captura de esa consola,
   sin mostrar contraseñas ni el contenido de `.env`.
4. **Frontend** — `pnpm dev` (puerto 5173, con proxy de `/api` al backend).
5. **Firmware** — copiar `config.example.h` a `config.h`, completar WiFi y
   credenciales MQTT, y ejecutar `pio run -t upload`.

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
