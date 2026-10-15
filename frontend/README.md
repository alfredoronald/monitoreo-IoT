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
VITE_API_URL=http://localhost:3000
```

Usar la dirección real del backend de cada equipo. Vite expone las variables
`VITE_*` al navegador: nunca colocar contraseñas ni tokens en ellas.
La tarea #81 implementará el cliente que consume esta dirección mediante
`import.meta.env.VITE_API_URL`. Reiniciar Vite al cambiar `.env`.
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
El layout usa tres columnas en laptop y una columna en móvil hasta 620 px.
Se comprobaron diez anchos entre 320 y 2560 px sin desplazamiento horizontal,
incluido el mínimo de 360 px requerido por la issue. La revisión final del
dashboard completo en dos navegadores corresponde a T-5.8 (#87).
Las tarjetas y los históricos muestran estados vacíos: la conexión a la API y
los gráficos se implementan en #83 y #84. No se necesita backend ni ESP32 para
comprobar esta maqueta. Evidencia: `docs/evidencias/t5-3/`.
