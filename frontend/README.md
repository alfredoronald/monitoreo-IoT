# Frontend — Monitoreo ambiental

Proyecto inicial de React + Vite + TypeScript para el dashboard del aula.
Incluye Recharts como dependencia para las tareas de gráficos posteriores.
Incluye el layout adaptable de HU-05 y el cliente API; las tarjetas dinámicas
y los gráficos se incorporan en las tareas posteriores.

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
Las tarjetas y los históricos muestran estados vacíos: la conexión a la API y
los gráficos se implementan en #83 y #84. No se necesita backend ni ESP32 para
comprobar esta maqueta. Evidencia: `docs/evidencias/t5-3/`.

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

Las pruebas usan respuestas controladas mediante fetch simulado, no sensores
ni endpoints reales. La UI inicial no se modifica para consumir datos todavía.

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
`docs/evidencias/t5-4/README.md`. Los históricos continúan pendientes de #84.
