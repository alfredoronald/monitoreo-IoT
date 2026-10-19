# Frontend — Monitoreo ambiental

Proyecto inicial de React + Vite + TypeScript para el dashboard del aula.
Incluye el layout adaptable de HU-05, el cliente API, las tarjetas dinámicas
y tres gráficos históricos de líneas con Recharts.

## Requisitos

- Node.js LTS 22.12 o superior (se verificó con Node.js 24).
- pnpm 10 para instalar las versiones de `pnpm-lock.yaml`.

## Instalación y arranque

Desde la raíz del repositorio, en Git Bash:

```bash
cd frontend
pnpm install --frozen-lockfile
cp -n .env.example .env
npm run dev
```

Abrir la dirección que imprime Vite, normalmente `http://localhost:5173`.
Detener el servidor con `Ctrl+C`.

En PowerShell, para copiar la plantilla sin sobrescribir una configuración local:

```powershell
if (-not (Test-Path .env)) { Copy-Item .env.example .env }
```

## Dirección de la API

Configurar en `frontend/.env`:

```dotenv
VITE_API_URL=http://localhost:3000/api
```

Usar la dirección real del backend de cada equipo. Vite expone las variables
`VITE_*` al navegador: nunca colocar contraseñas ni tokens en ellas.
El cliente consume esta dirección mediante `import.meta.env.VITE_API_URL`.
Incluir `/api`: el cliente añade `/lecturas`, `/alertas`, etc.
Reiniciar Vite al cambiar `.env`.
El backend no es necesario para abrir la pantalla inicial de esta tarea.

`.env` y las variantes locales están ignoradas por Git; `.env.example` sí se
versiona y solo contiene una dirección de ejemplo.

## Comprobaciones

```bash
npm run lint
npm run build
pnpm list recharts --depth 0
```

`build` comprueba TypeScript y genera `dist/`, excluido de Git. La evidencia de
T-5.1 se guarda en `docs/evidencias/t5-1/` y sigue la rama de frontend.

## Layout del dashboard (T-5.3, #82)

La pantalla contiene encabezado, navegación, tres tarjetas y áreas de histórico.
El layout usa tres columnas por encima de 900 px y una columna hasta 900 px,
para evitar tarjetas estrechas en celulares y tablets. Usa tipografía del sistema
(`system-ui`), sin depender de fuentes externas, y textos secundarios de 14 px.
Se comprobaron doce anchos entre 320 y 2560 px sin desplazamiento horizontal,
incluido el mínimo de 360 px requerido por la issue. La revisión final del
dashboard completo en dos navegadores corresponde a T-5.8 (#87).
Las tarjetas se conectan a la API en #83 y los históricos en #84. La evidencia
del layout inicial está en `docs/evidencias/t5-3/`.

## Cliente de la API (T-5.2, #81)

Referencia: [contrato propuesto de PR #103](https://github.com/alfredoronald/monitoreo-IoT/blob/7af6e70bc46c1af8723cf902dd3e6f0565ebccde/docs/api.md).
El contrato sigue en revisión; comprobar cambios antes de fusionar esta tarea.

```ts
import { api, ApiError } from './api'

const latest = await api.getLatestReadings()
const history = await api.getReadings({
  variable: 'temperatura',
  desde: '2026-10-15T08:00:00-04:00',
  hasta: '2026-10-15T09:00:00-04:00',
})
const state = await api.getAirState() // objeto o null
const pending = await api.getAlerts(false) // sin argumento: todas
const attended = await api.markAlertAttended('7')
```

Todas las funciones aceptan opciones `{ signal }` como último argumento para
cancelar solicitudes. Los errores HTTP lanzan `ApiError` con `status` y el
`mensaje` del backend (o un mensaje genérico si no hay JSON).
Los fallos de red y la cancelación mantienen su error original.
Se validan las respuestas antes de devolverlas al dashboard: listas vacías y
estado `null` son válidos; NUMERIC como texto no lo es. Los IDs son strings,
sin convertir BIGINT a Number. Las fechas de salida deben ser UTC con milisegundos.

```bash
npm test
```

Las pruebas del cliente usan respuestas controladas mediante fetch simulado,
no sensores ni endpoints reales.

## Tarjetas actuales (T-5.4, #83)

Configurar `VITE_API_URL` en `.env` con el prefijo del backend real, por ejemplo
`http://localhost:3000/api`, y reiniciar Vite. El backend debe permitir el origen
exacto del frontend mediante CORS. No colocar credenciales en variables VITE_*.

Las tarjetas consultan `GET /api/lecturas/ultima` al abrir y cada 15 segundos.
Muestran valor, unidad y hora de `fecha_hora` en la zona horaria del dispositivo.
Si falta una variable se muestra un guion; si falla una consulta se conserva el
último valor con su hora original y se informa del fallo. Al salir se cancelan
el temporizador y la petición activa. El cero es un valor válido.

Validación: `npm test`, `npm run lint` y `npm run build`.
Se probó contra el backend real de #76 y PostgreSQL usando lecturas sintéticas
en un esquema transaccional aislado, sin ESP32. Procedimiento y video:
`docs/evidencias/t5-4/README.md`.

## Gráficos históricos (T-5.5, #84)

El dashboard consulta `GET /api/lecturas` para `temperatura`, `humedad` y `co2`
al abrir el histórico. Las tres consultas comparten inicialmente un rango
explícito desde una hora antes hasta el instante de apertura. El selector de
#85 permite cambiarlo; el histórico no utiliza el polling de las tarjetas.
Se necesitan el backend histórico #74, datos en ese intervalo y CORS configurado
para el origen de Vite. No se generan lecturas de ejemplo en la aplicación.

Cada gráfico conserva los colores y el layout de #82. El eje X muestra fecha y
hora con escala temporal numérica: la distancia entre puntos refleja el tiempo
transcurrido. El eje Y incluye °C, % HR o ppm. El tooltip muestra el valor sin
redondearlo a un decimal y la fecha/hora completa con milisegundos y zona horaria.
Las horas usan la zona del dispositivo; `fecha_hora` UTC se conserva sin cambios.
Un único punto se muestra con un marcador y el cero es un valor válido.

Cada variable tiene estados independientes de carga, error y rango sin lecturas.
Las peticiones tienen un timeout de 10 segundos. Cambiar el rango o desmontar el
componente cancela las consultas con `AbortSignal` e ignora respuestas tardías.
Los fallos de una variable no ocultan los resultados de las otras.

`HistoricalCharts` acepta un rango controlado, sin modificar el cliente API:

```tsx
<HistoricalCharts range={{ desde: '2026-10-15T12:00:00.000Z', hasta: '2026-10-15T13:00:00.000Z' }} />
```

Enviar fechas ISO válidas con zona y `desde <= hasta`. Omitir `range` utiliza
la última hora inicial; el dashboard lo controla mediante `HistoryPanel`.

Validación desde `frontend/`: `npm test`, `npm run lint` y `npm run build`.
Las pruebas adicionales cubren las tres consultas con el cliente real y fetch
simulado, errores parciales, listas vacías, cambios de rango, cancelación,
timeout, intervalos irregulares, conservación de datos, formatos y renderizado
de mensajes y tooltip. No requieren PostgreSQL ni ESP32.

## Selector de periodo (T-5.6, #85)

`HistoryPanel` conecta `HistoryRangeSelector` con los tres gráficos existentes:

- **Última hora**: rango inicial; al seleccionarlo calcula los últimos 60 minutos.
- **Últimas 24 horas**: aplica inmediatamente 24 horas transcurridas, incluso si
  hay un cambio de horario en la zona del dispositivo.
- **Personalizado**: permite editar Desde y Hasta, con precisión de segundos.
  No se consulta la API hasta pulsar **Aplicar rango** o enviar el formulario
  con el teclado y fechas válidas. El rango aplicado se sigue mostrando debajo.

Ambos periodos rápidos toman una sola lectura del reloj para sus extremos.
Las fechas personalizadas se interpretan en la zona del dispositivo y se envían
como ISO UTC con segundos y zona explícita. Se rechazan campos vacíos, fechas
inexistentes, horas locales inexistentes por cambio de horario y rangos
invertidos. Se permiten extremos iguales. Los errores identifican el campo y
el foco se mueve al primero que necesita corrección.

Los cambios de rango reutilizan `useHistoricalReadings`: tres peticiones
independientes, cancelación de las anteriores y estados por variable. El
selector no envía peticiones ni introduce datos de demostración.

Las pruebas de cálculo y validación forman parte de `npm test`. Para comprobar
también interacción, teclado, solicitudes HTTP controladas y diseño responsive
con Chrome, Chromium o Edge instalados, ejecutar desde `frontend/`:

```powershell
npm.cmd run test:browser:history-range
```

Si el navegador está en otra ubicación, configurar `CHROME_PATH` con su ruta.
La prueba utiliza un perfil temporal y datos sintéticos solo dentro de la
verificación. No necesita ni modifica un backend real. Las capturas y el informe
se guardan en `frontend/node_modules/.tmp/t5-6/`; agregar `-- --evidence` al
comando los guarda en `docs/evidencias/t5-6/`.
