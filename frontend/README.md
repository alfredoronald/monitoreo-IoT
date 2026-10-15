# Frontend — Monitoreo ambiental

Proyecto inicial de React + Vite + TypeScript para el dashboard del aula.
Incluye Recharts como dependencia para las tareas de gráficos posteriores.
La pantalla inicial de Vite permite comprobar el arranque y la interacción;
todavía no consulta lecturas ni implementa el dashboard de HU-05.

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
