# T-5.6 — Selector de rango conectado a la API

Issue: #85. Rama: `feat/85-history-range`.
Dependencia: #84 (`feat/84-history-charts`, PR #114).

## Comportamiento implementado

- Última hora y últimas 24 horas aplican el rango inmediatamente, tomando un
  solo instante actual para calcular ambos extremos.
- Personalizado ofrece Desde, Hasta y Aplicar rango. La edición no modifica
  el rango aplicado ni genera consultas hasta enviar fechas válidas.
- Las fechas usan la zona del dispositivo, con precisión de segundos; la API
  recibe ISO UTC. Se rechazan campos vacíos, fechas u horas inexistentes y
  rangos invertidos. Se permite `desde === hasta`.
- Los cambios reutilizan `HistoricalCharts`, `useHistoricalReadings` y el
  cliente existente: tres consultas por rango, cancelación de las anteriores
  y estados independientes de carga, vacío y error.
- Etiquetas asociadas, mensajes por campo, foco en el campo inválido y uso
  mediante Tab y Enter. Controles apilados en móvil.

## Origen de los datos y capturas

Las seis capturas corresponden al frontend real ejecutado en **Chrome
headless**, a 1366 y 360 px. Los datos son **controlados y sintéticos**, generados
solo por `frontend/tests/history-range.browser.mjs`. Las respuestas HTTP de
`GET /api/lecturas` se interceptan mediante Chrome DevTools Protocol.

No se utilizó un backend real, PostgreSQL ni ESP32. Las tarjetas actuales
recibieron una lista vacía. El código de producción no incorpora datos de
demostración ni interceptores de peticiones.

| Periodo | Laptop, 1366 px | Celular, 360 px |
|---|---|---|
| Última hora | [Captura](ultima-hora-1366.png) | [Captura](ultima-hora-360.png) |
| Últimas 24 horas | [Captura](ultimas-24-horas-1366.png) | [Captura](ultimas-24-horas-360.png) |
| Personalizado | [Captura](personalizado-1366.png) | [Captura](personalizado-360.png) |

El periodo de 24 horas utiliza 5.760 puntos por variable. El personalizado de
las capturas va del 15/10/2026 a las 08:00:00 hasta las 10:00:00, en la zona del
dispositivo (GMT-4). Los extremos ISO realmente aplicados aparecen debajo del
selector, formateados en hora local.

## Pruebas y reproducción

Entorno: Node.js 24.22.0, React 19.3.0 y Recharts 3.10.1.
Desde `frontend/`:

```powershell
npm.cmd test
npm.cmd run lint
npm.cmd run build
npm.cmd run test:browser:history-range -- --evidence
```

La prueba de navegador requiere Chrome, Edge o Chromium instalado. Si no está
en una ruta reconocida, configurar `CHROME_PATH`. Crea un perfil temporal,
levanta Vite y controla únicamente las respuestas usadas por la prueba.

Resultados:

- Suite completa: **36 pruebas aprobadas**, incluidas 6 nuevas de cálculo y
  validación. Sin cambios a las pruebas anteriores de carga y cancelación.
- Lint sin advertencias y build TypeScript/Vite correcto.
- Rangos de 60 minutos y 24 horas exactos, cambios de día, conversión en
  America/La_Paz y Asia/Kolkata, y hora inexistente por cambio de horario en
  America/New_York.
- Navegador: tres consultas con extremos comunes por cambio aplicado; ninguna
  durante la edición ni tras enviar fechas inválidas; envío válido con Enter.
- Extremos iguales: tres peticiones válidas y tres gráficos con un solo punto.
- Cancelación de las tres peticiones retenidas del rango anterior; sus
  respuestas tardías no alteran las curvas del rango nuevo.
- Carga visible, tres respuestas vacías y un error HTTP 500 de humedad sin
  ocultar los otros dos gráficos.
- Tab recorre inicio, fin y botón; el error dirige el foco al campo inválido.
- Sin desplazamiento horizontal a 360 o 1366 px ni excepciones de JavaScript.

El informe generado por la prueba se conserva en
[resultados-navegador.json](resultados-navegador.json), junto con las capturas.
La revisión completa del dashboard en dos navegadores corresponde a #87.
