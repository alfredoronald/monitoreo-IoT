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
| `develop` | Rama de integración continua donde se fusionan los avances de los módulos |
| `feature/firmware-esp32` | Lecturas de sensores y protocolo PubSubClient |
| `feature/backend-mqtt-api` | Suscriptor Node.js, conexiones PostgreSQL y endpoints Express |
| `feature/frontend-dashboard` | Componentes React, gráficos Recharts y hojas de estilos |

## Cómo levantar cada parte

> Próximos pasos: los archivos de configuración y código se agregan en la
> task de implementación.

1. **Base de datos** — crear la base `monitoreo_ambiental` y el usuario de
   la app; después ejecutar el script de `database/` con DBeaver o `psql`.
2. **Broker MQTT** — instalar Mosquitto, copiar la configuración de
   `infra/mosquitto/` y crear los usuarios (`esp32` y `api`) con
   `mosquitto_passwd`. Sin usuarios no hay conexión: desde la versión 2.0,
   al definir un `listener` el broker deja de aceptar clientes anónimos.
3. **Backend** — `pnpm install`, completar `backend/.env` (copiado de
   `.env.example`) y `pnpm dev` (puerto 3000).
4. **Frontend** — `pnpm dev` (puerto 5173, con proxy de `/api` al backend).
5. **Firmware** — en VS Code con PlatformIO IDE, abrir PlatformIO Home,
   elegir **Open Project** y seleccionar la carpeta `firmware/`, donde
   está `platformio.ini`. Abrir una terminal de PlatformIO en esa carpeta.

   Crear la configuración privada copiando la plantilla (PowerShell):

   ```powershell
   Copy-Item include/config.example.h include/config.h
   ```

   Completar en `include/config.h` el SSID, la contraseña WiFi y el host,
   puerto, usuario y contraseña del broker. Este archivo está excluido
   por `.gitignore`. El arranque inicial solo imprime un mensaje serie;
   la configuración se utilizará en las próximas tareas de conexión.

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
   el mensaje `Firmware ESP32 iniciado`. Salir del monitor con `Ctrl+C`.

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
