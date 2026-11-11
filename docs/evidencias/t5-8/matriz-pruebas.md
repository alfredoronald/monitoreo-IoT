# Matriz ejecutada — T-5.8 (#87)

Fecha local: 21/10/2026, America/La_Paz (UTC−4). Los informes conservan timestamps ISO UTC del 22/10; corresponden a la noche local del día 21.

Commit base: `9913129e2cd9a05b7858420825ea3c2b39d55d67`, con cambios locales cuyo SHA-256 se verificó al consolidar los informes. Chrome 155.0.8059.86 y Edge 155.0.4283.62. Datos sintéticos interceptados por CDP; no hay teléfono físico ni API real en esta matriz.

## Cierre completo

Comando ejecutado: `npm.cmd run test:browser:responsive -- --phase=cierre`. Matriz sin filtro: 21 escenarios por combinación, nueve comprobaciones adicionales de breakpoints por navegador y una comprobación global de consola por navegador. Total: **146 PASS, 0 FAIL**.

| Navegador | Viewport CSS | Escenarios | PASS | FAIL | Captura representativa |
|---|---|---:|---:|---:|---|
| chrome | 360×800 | 21 | 21 | 0 | [Dashboard](capturas/chrome/360x800/cierre/01-dashboard-ultima-hora.png) |
| chrome | 768×1024 | 21 | 21 | 0 | [Dashboard](capturas/chrome/768x1024/cierre/01-dashboard-ultima-hora.png) |
| chrome | 1366×768 | 21 | 21 | 0 | [Dashboard](capturas/chrome/1366x768/cierre/01-dashboard-ultima-hora.png) |
| edge | 360×800 | 21 | 21 | 0 | [Dashboard](capturas/edge/360x800/cierre/01-dashboard-ultima-hora.png) |
| edge | 768×1024 | 21 | 21 | 0 | [Dashboard](capturas/edge/768x1024/cierre/01-dashboard-ultima-hora.png) |
| edge | 1366×768 | 21 | 21 | 0 | [Dashboard](capturas/edge/1366x768/cierre/01-dashboard-ultima-hora.png) |

## Resultados por escenario

Las celdas con PNG seleccionado enlazan la captura real correspondiente. Las marcadas «captura archivada» llevan a la guía del respaldo local; esa imagen no está directamente en Git. El [catálogo actual](catalogo-capturas.csv) identifica la ruta original por navegador, resolución y escenario. Los JSON se conservan sin cambios y contienen aserciones, medidas DOM, fechas y solicitudes. Los PASS/FAIL de esta matriz no se alteran al seleccionar imágenes. Se mantienen seis capturas con tooltip activo en la selección.

| Escenario | Chrome 360 | Chrome 768 | Chrome 1366 | Edge 360 | Edge 768 | Edge 1366 |
|---|---|---|---|---|---|---|
| 01-dashboard-ultima-hora | [PASS](capturas/chrome/360x800/cierre/01-dashboard-ultima-hora.png) | [PASS](capturas/chrome/768x1024/cierre/01-dashboard-ultima-hora.png) | [PASS](capturas/chrome/1366x768/cierre/01-dashboard-ultima-hora.png) | [PASS](capturas/edge/360x800/cierre/01-dashboard-ultima-hora.png) | [PASS](capturas/edge/768x1024/cierre/01-dashboard-ultima-hora.png) | [PASS](capturas/edge/1366x768/cierre/01-dashboard-ultima-hora.png) |
| 02-tooltips-tres-graficos | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 03-24-horas-5760-puntos | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/chrome/1366x768/cierre/03-24-horas-5760-puntos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 04-personalizado-valido-teclado | [PASS](capturas/chrome/360x800/cierre/04-personalizado-valido-teclado.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/chrome/1366x768/cierre/04-personalizado-valido-teclado.png) | [PASS](capturas/edge/360x800/cierre/04-personalizado-valido-teclado.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/1366x768/cierre/04-personalizado-valido-teclado.png) |
| 05-fechas-vacias-invalidas | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 06-fechas-invertidas | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 07-fechas-iguales-un-punto | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/07-fechas-iguales-un-punto.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 08-cambio-rapido-cancelacion | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 09-cargando | [PASS](capturas/chrome/360x800/cierre/09-cargando.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/09-cargando.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 10-error-inicial | [PASS](capturas/chrome/360x800/cierre/10-error-inicial.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/10-error-inicial.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 11-sin-datos | [PASS](capturas/chrome/360x800/cierre/11-sin-datos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/11-sin-datos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 12-datos-antiguos | [PASS](capturas/chrome/360x800/cierre/12-datos-antiguos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/12-datos-antiguos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 13-error-parcial | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 14-aire-bueno | [PASS](capturas/chrome/360x800/cierre/14-aire-bueno.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/14-aire-bueno.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 14-aire-moderado | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 14-aire-malo | [PASS](capturas/chrome/360x800/cierre/14-aire-malo.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/14-aire-malo.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 15-alertas-patch-fallido-reintento | [PASS](capturas/chrome/360x800/cierre/15-alertas-patch-fallido-reintento.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/15-alertas-patch-fallido-reintento.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 16-navegacion-foco-controles | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 17-valores-numericos-largos | [PASS](capturas/chrome/360x800/cierre/17-valores-numericos-largos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/17-valores-numericos-largos.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 18-textos-aire-sin-espacios | [PASS](capturas/chrome/360x800/cierre/18-textos-aire-sin-espacios.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/18-textos-aire-sin-espacios.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 19-polling-error-conserva-recuperacion | [PASS](capturas/chrome/360x800/cierre/19-polling-error-conserva-recuperacion.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/360x800/cierre/19-polling-error-conserva-recuperacion.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |

## Breakpoints

En cada ancho se verificaron también geometría, overflow y solapamientos. A 600 px el banner cambia de columna a fila al superar ese valor; hasta 620 px Aplicar ocupa todo el ancho; hasta 900 px las tarjetas y los campos personalizados usan una columna.

| Viewport | Chrome | Edge |
|---|---|---|
| 599×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 600×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/600x900/cierre/20-breakpoint.png) |
| 601×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 619×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 620×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 621×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS](capturas/edge/621x900/cierre/20-breakpoint.png) |
| 899×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 900×900 | [PASS · captura archivada](archivo-local.md#capturas-archivadas) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |
| 901×900 | [PASS](capturas/chrome/901x900/cierre/20-breakpoint.png) | [PASS · captura archivada](archivo-local.md#capturas-archivadas) |

La comprobación global de excepciones JavaScript y errores de consola inesperados pasó en ambos navegadores. Los HTTP 500 provocados por los fixtures son esperados y se registran por separado.

## Antes y después de las correcciones

| Defecto | Antes, Chrome 360 | Después Chrome 360 | Después Edge 360 |
|---|---|---|---|
| D87-01 | 480px / 360px: FAIL | 345px / 360px: PASS | 345px / 360px: PASS |
| D87-02 | 2870px / 360px: FAIL | 345px / 360px: PASS | 345px / 360px: PASS |

Los dos escenarios también aprobaron a 768 y 1366 px en ambos navegadores. Detalle, pasos y capturas: [defectos.md](defectos.md).

## Historial de ejecuciones, sin ocultar fallos

| Informe | Escenarios ejecutados | PASS | FAIL |
|---|---:|---:|---:|
| [resultados-chrome-antes.json](resultados-chrome-antes.json) | 22 | 18 | 4 |
| [resultados-chrome-integracion-inicial.json](resultados-chrome-integracion-inicial.json) | 73 | 72 | 1 |
| [resultados-edge-integracion-inicial.json](resultados-edge-integracion-inicial.json) | 73 | 72 | 1 |
| [resultados-chrome-regresion.json](resultados-chrome-regresion.json) | 73 | 71 | 2 |
| [resultados-edge-regresion.json](resultados-edge-regresion.json) | 73 | 71 | 2 |
| [resultados-chrome-tooltips.json](resultados-chrome-tooltips.json) | 6 | 6 | 0 |
| [resultados-edge-tooltips.json](resultados-edge-tooltips.json) | 6 | 6 | 0 |
| [resultados-chrome-cierre.json](resultados-chrome-cierre.json) | 73 | 73 | 0 |
| [resultados-edge-cierre.json](resultados-edge-cierre.json) | 73 | 73 | 0 |
| Total de todos los intentos, incluidas repeticiones | 472 | 462 | 10 |

Los 10 FAIL históricos incluyen dos defectos del producto y ocho fallos de la herramienta de prueba durante su preparación; están explicados en defectos.md. La fase interna `final` de los dos informes archivados `integracion-inicial` fue un primer intento, no el cierre aceptado. Los alias resultados-chrome.json y resultados-edge.json son copias exactas de cierre. Las copias no se cuentan dos veces. El intento inicial de conexión CDP restringida agotó el tiempo antes de ejecutar escenarios; no se incluye en los totales de escenarios.

## Validación y revisión visual

- `npm.cmd test`: 46 aprobadas, 0 fallidas, 0 omitidas; código 0. Duración registrada: 15560.0793 ms.
- `npm.cmd run lint`: código 0, también después del último ajuste del script.
- `npm.cmd run build`: código 0; TypeScript y Vite 8.3.2; 611 módulos transformados.
- Logs y tiempos exactos: [validacion.json](validacion.json), [test.txt](test.txt), [lint.txt](lint.txt), [lint-cierre.txt](lint-cierre.txt), [build.txt](build.txt).

La inspección visual descrita corresponde a la ejecución original; algunas de esas capturas están ahora en el respaldo local. Se inspeccionaron visualmente capturas representativas de las seis combinaciones: dashboard completo en Chrome 360 y 1366, personalizado con foco en Chrome 768, dashboard de Edge 360, personalizado de Edge 768 y tooltip de Edge 1366. Además se revisaron el overflow numérico antes/después, el texto largo corregido y el breakpoint de 901 px. Se observan apilado, unidades, fechas, foco y gráficos legibles; el scroll vertical de las capturas completas es esperado. No se afirma haber inspeccionado manualmente cada PNG.

Pendiente fuera de esta matriz: teléfono físico, teclado/calendario móvil nativo, Safari/Firefox y conexión/persistencia reales. Esta matriz no sustituye esas pruebas.
