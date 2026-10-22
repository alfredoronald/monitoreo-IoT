# T-5.7 — Estados del dashboard (#86)

Fecha de verificación: 18 de octubre de 2026.
Rama de trabajo: `feat/86-loading-error-states`.
Destino previsto: `feature/frontend-dashboard`.

## Alcance y procedencia de los datos

**Todas las respuestas de estas evidencias son controladas y sintéticas.**
Se generan exclusivamente en `frontend/tests/dashboard-states.browser.mjs`,
mediante interceptación HTTP del navegador para `controlled-api.test`.
La aplicación de producción no contiene lecturas de demostración ni mocks.
No se utilizaron ESP32, sensores, PostgreSQL ni un backend real y no se cambió
el contrato API. La variable de entorno de prueba solo existe en el proceso;
no se modifica ningún archivo `.env`.

Esta entrega conserva tarjetas y polling (#83), gráficos (#84), indicador
de calidad del aire (#91) y alertas (#92). No incorpora cambios de #85 ni
del PR #118 pendiente.

## Escenarios y capturas

| Escenario | Respuesta controlada y resultado | Escritorio | Móvil |
|---|---|---|---|
| Cargando | Solicitudes retenidas; mensajes de carga en tarjetas, indicador e históricos | [1366 px](cargando-1366.png) | [360 px](cargando-360.png) |
| Error inicial | HTTP 500 sin muestra previa; mensajes de error, sin confundirlos con una respuesta vacía | [1366 px](error-1366.png) | [360 px](error-360.png) |
| Sin datos | HTTP 200 con listas vacías y estado de aire `null`; mensajes de ausencia de datos | [1366 px](sin-datos-1366.png) | [360 px](sin-datos-360.png) |
| Sin datos recientes | HTTP 200 con muestras de hace dos minutos; valor, unidad y fecha conservados como última lectura conocida | [1366 px](sin-datos-recientes-1366.png) | [360 px](sin-datos-recientes-360.png) |
| Error con datos anteriores | Falla el siguiente polling; se conservan valores y fechas y coexisten error y antigüedad | — | [360 px](error-con-ultima-lectura-360.png) |
| Recuperación | El siguiente polling recibe muestras nuevas; desaparecen error y antigüedad y regresa la clasificación actual | — | [360 px](recuperacion-360.png) |

Los históricos de las capturas contienen muestras anteriores a un minuto y
siguen siendo gráficos válidos. La humedad incluye el valor **0**, sin
convertirlo en ausencia de datos. Las capturas están completas, sin recortes.

## Comprobaciones automatizadas

Desde `frontend/`:

```powershell
npm.cmd test
npm.cmd run lint
npm.cmd run build
npm.cmd run test:browser:dashboard-states -- --evidence
```

- `npm test`: **40 aprobadas, 0 fallidas**. Incluye límites 59 999, 60 000 y
  60 001 ms, antigüedad por variable, reloj sin respuestas HTTP, cancelación,
  recuperación y conservación de niveles, etiquetas, recomendaciones y colores.
  Se conservan las pruebas de API, polling, históricos y atención de alertas.
- `npm run lint`: correcto, sin advertencias.
- `npm run build`: correcto; TypeScript y compilación de producción.
- Navegador: Chrome sin interfaz, con anchos de **360 y 1366 px**.
  El detalle de versión, intervalos medidos y resultado está en
  [resultados-navegador.json](resultados-navegador.json).

El escenario de antigüedad independiente entrega temperatura cercana al minuto,
humedad reciente y CO₂ antiguo. La temperatura y la clasificación caducan con
el reloj de interfaz antes de la siguiente petición HTTP, mientras la humedad
continúa reciente. Se verifica que los timestamps no cambian. Luego se provoca
un error de consulta y finalmente se permite la recuperación en el polling.

También se comprueba una variable ausente y tres estados históricos simultáneos:
temperatura con datos, humedad con error y CO₂ vacío. La antigüedad de las tarjetas
no altera estos resultados. El polling se mide con tiempo real, alrededor de
15 segundos; los límites exactos se prueban con reloj simulado.

En ambos anchos se comprueba ausencia de desplazamiento horizontal, foco visible
en «Saltar al contenido», navegación con Tab y Enter y atención de una alerta
mediante teclado. Atenderla conserva el indicador y los tres históricos.
Las capturas de escritorio y móvil se inspeccionan visualmente para revisar
legibilidad, ajustes de línea y conservación de componentes.

## Limitaciones e integración

- La verificación automatizada usa un navegador de escritorio sin interfaz.
  La emulación de 360 px no sustituye una prueba en un teléfono físico.
- No se valida conectividad real con el ESP32 ni condiciones de red de campo.
  Un dato antiguo tampoco permite afirmar que el sensor está desconectado.
- La regla depende del reloj del dispositivo y del timestamp recibido. Los
  navegadores pueden demorar temporizadores en pestañas suspendidas; al volver
  a la pestaña se recalcula la antigüedad.
- La revisión completa en dos navegadores y dispositivos corresponde a #87.
- Al integrar PR #118, conservar `HistoryPanel` en el histórico y estos cambios
  del resumen. Revisar posibles solapamientos en `App.tsx`, `App.css`,
  `package.json` y `frontend/README.md`, preservando ambos scripts de navegador.
