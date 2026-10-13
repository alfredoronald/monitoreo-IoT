# Evidencias de integración #32

## Resultado

El 13 de octubre de 2026 se ejecutó una prueba física de **5 minutos**:
ESP32 → Mosquitto → backend → PostgreSQL. Se guardaron **60 de 60 mensajes**,
sin diferencias de valores ni eventos de fallo dentro de la ventana observada.

La prueba registra el funcionamiento durante 300,191 segundos y documenta las
cuatro etapas del flujo mediante evidencias PNG, registros e informe de comparación.

- [Informe](prueba-5min/informe.md)
- [Inicio](prueba-5min/inicio.png)
- [Punto intermedio, aproximadamente 2 min 30 s](prueba-5min/intermedio.png)
- [Final](prueba-5min/final.png)
- [Comparación](prueba-5min/comparacion.png)
- [Resultado estructurado](prueba-5min/final.json)
- [Registros comprimidos con SHA-256](prueba-5min/registros.zip)
- [Preparación e incidencia previa](preparacion.md)

Los PNG son **reportes generados desde registros reales**, no capturas de pantalla.
El ZIP incluye serial ESP32, recepción MQTT, recepción backend, salud de procesos,
filas de PostgreSQL, cortes inicial/intermedio/final y la versión de la herramienta utilizada.

## Reproducir la herramienta

1. Trabajar en una copia de la versión integrada `c9f2d2d`, indicada en el informe.
2. Levantar Mosquitto, backend y PostgreSQL; usar el ESP32 físico como único publicador.
3. Completar las credenciales locales en los archivos privados habituales.
4. Extraer el ZIP y copiar `herramientas/backend/scripts/` a `backend/scripts/`.
5. Usar Python con `pyserial` y `Pillow`; instalar las dependencias Node del backend.
6. En una terminal ubicada en `backend`, ejecutar:

```powershell
$env:E2E_PYTHON = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$env:E2E_SERIAL_PORT = "COM5"
$env:E2E_DURATION_SECONDS = "300"
$env:E2E_RUN_NAME = "otra-prueba-5min"
pnpm exec tsx scripts/endToEnd32.ts
```

Usar un nombre de ejecución nuevo para conservar cada intento. Los parámetros
`E2E_*` controlan únicamente esta herramienta; no modifican la configuración de la aplicación.

## Configuración de VS Code

Abrir `monitoreo-IoT.code-workspace` para que `firmware` tenga su propia raíz.
Desde `firmware`, generar la configuración de C/C++ con
`pio project init --ide vscode --environment esp32dev`. La configuración generada
utiliza las rutas locales de cada computador y no se versiona.
