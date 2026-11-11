# T-5.8 — Prueba integral responsive (#87)

Responsable: ErickAr-Dev. Rama: `test/87-responsive-check`. Base y destino: `feature/frontend-dashboard`.

## Alcance y entorno

Se prueba el dashboard React real con las funcionalidades integradas de #83, #84, #85, #86, #91 y #92. El único cambio de producción está en CSS; no se modifican contratos API, backend ni firmware.

La revisión Git de partida es `9913129e2cd9a05b7858420825ea3c2b39d55d67`. Las pruebas incluyen cambios locales sin commit: los informes JSON registran el estado Git al empezar y SHA-256 de la prueba y de los archivos visuales principales. El commit por sí solo no identifica las correcciones locales.

Entorno: Windows, Node.js 24.22.0, React 19.3.0, Recharts 3.10.1 y Vite 8.3.2. La fecha local de ejecución y la zona se detallan en los informes; los timestamps de máquina están en ISO UTC y el navegador usa explícitamente `America/La_Paz` (UTC−4).

Ejecutables comprobados: Chrome **155.0.8059.86** y Microsoft Edge **155.0.4283.62**. Sus versiones también fueron consultadas mediante CDP, no sólo desde el archivo instalado. Metadatos: [entorno.json](entorno.json).

Resultado de cierre del **21 de octubre de 2026**: **146 escenarios aprobados y 0 fallidos**, 73 por navegador. Incluye las seis combinaciones solicitadas y los anchos adyacentes a los tres breakpoints. Los dos defectos de overflow se corrigieron con cambios mínimos en `frontend/src/App.css`. `npm.cmd test`: 46/46; lint y build: código 0.

Entrega reducida el **22 de octubre de 2026**: 45 capturas representativas disponibles directamente en Git. Se conservaron los informes y logs completos sin modificar y los once archivos de auditoría previa. Los 209 originales se copiaron fuera del repositorio y se verificaron por SHA-256 antes de retirar los 142 archivos listados. La [documentación del archivo local](archivo-local.md) y el [catálogo actual](catalogo-capturas.csv) identifican qué imágenes requieren acceso al respaldo. Esta tarea no repitió pruebas ni cambió resultados.

## Procedencia de datos

**Datos sintéticos y respuestas HTTP controladas, exclusivamente dentro de la prueba.** El frontend es servido por Vite; CDP intercepta todas las rutas de `http://controlled-api.test/api/*`, incluidos GET, OPTIONS y PATCH. No se usan sensores, base de datos ni backend reales. Los mensajes largos y números extremos son casos de estrés válidos según el contrato, no mediciones ambientales reales.

El patrón de transporte CDP, servidor Vite, perfil temporal e interceptación Fetch se reutiliza de `history-range.browser.mjs` (#85) y `dashboard-states.browser.mjs` (#86). Ambos archivos y sus evidencias permanecen intactos. La prueba nueva es `frontend/tests/responsive-dashboard.browser.mjs`.

`VITE_API_URL` sólo se establece en el proceso de prueba. Vite no carga archivos `.env` (`envDir: false`); no se crea ni modifica `frontend/.env`. Cada navegador usa un perfil temporal propio, eliminado al terminar después de verificar su ruta. No se instalan dependencias nuevas.

## Reproducción

Desde `frontend/`, con las dependencias ya instaladas:

```powershell
npm.cmd run test:browser:responsive
# Un navegador y dimensiones concretas:
npm.cmd run test:browser:responsive -- --browser=edge --viewports=360x800,1366x768 --breakpoints=no --phase=revision
# Regresión completa con capturas separadas de la primera ejecución:
npm.cmd run test:browser:responsive -- --phase=cierre
# Diagnóstico rápido de tooltips; estos dos escenarios son independientes del resto:
npm.cmd run test:browser:responsive -- --phase=tooltips-nuevo --breakpoints=no --scenarios=01-dashboard-ultima-hora,02-tooltips-tres-graficos
```

`--browser` acepta `all`, `chrome` o `edge`. Los viewports por defecto son `360x800,768x1024,1366x768`. Por defecto también se prueban 599, 600, 601, 619, 620, 621, 899, 900 y 901 px, con alto de 900 px. `CHROME_PATH` y `EDGE_PATH` permiten indicar otro ejecutable instalado. `--phase` separa informes y capturas de diferentes ejecuciones; usar un nombre nuevo para conservar cada intento.

`--scenarios` filtra identificadores exactos separados por comas para diagnóstico; los casos de fechas 05–08 dependen del estado establecido por 01–04 y deben ejecutarse juntos. La matriz de cierre se ejecuta **sin filtro**. Los alias `resultados-chrome.json` y `resultados-edge.json` se actualizan con copias exactas de los informes de `cierre` sólo después de que termine esa fase.

La prueba sale con código 1 si falla un escenario. Guarda el resultado real de cada escenario, sus dimensiones, mediciones DOM, captura, error si existe, consultas interceptadas, respuestas, cancelaciones, errores de consola y versión CDP. Los 500 que se provocan expresamente en los escenarios de error se distinguen de las excepciones de JavaScript.

## Validación y evidencias

- [Matriz ejecutada](matriz-pruebas.md).
- [Defectos y comprobación antes/después](defectos.md).
- [Chrome](resultados-chrome.json) y [Edge](resultados-edge.json).
- [Prueba inicial de defectos en Chrome](resultados-chrome-antes.json).
- [Registro de tests](test.txt), [lint](lint.txt), [build](build.txt) y [códigos de salida](validacion.json).
- Las 45 capturas seleccionadas conservan sus rutas bajo `capturas/{chrome,edge}/{ancho}x{alto}/{fase}/`; la [lista auditada](revision-entrega/conservar-en-repositorio.csv) enumera los archivos conservados.
- [Resumen consolidado original](resumen.json): cierre y todos los intentos, sin contar dos veces los alias. Sus cantidades de PNG describen la fase anterior a la reducción.
- [Verificación de reducción](reduccion-verificacion.json): integridad del respaldo, archivos retirados y controles de la entrega actual.

De las **569 capturas distintas originales**, **45 están disponibles directamente en Git** y **524 requieren el respaldo local**. Los dos PNG anteriores a las correcciones y sus cuatro comparaciones posteriores permanecen visibles en Git. El ZIP de diagnóstico ya no está en el repositorio: se conserva intacto, junto con los 209 archivos originales, en `D:\telematicos\respaldos-IoT\t5-8-completo\`. Esta ruta es local y **no estará disponible automáticamente para otros colaboradores**. No se publicó un artefacto compartido.

El [catálogo de capturas](catalogo-capturas.csv) permite localizar cada ruta original sin modificar los JSON. La [guía de recuperación](archivo-local.md#recuperación-e-integridad) explica cómo consultar los PNG sueltos y las entradas del ZIP externo. El [manifiesto original](capturas-diagnostico-manifiesto.json) conserva tamaños y SHA-256 de las 393 entradas del ZIP. No se modificó el contenido de ninguna imagen.

Todos los intentos suman **472 ejecuciones de escenarios: 462 PASS y 10 FAIL históricos**. Dos FAIL corresponden a los defectos del producto; ocho a errores de la prueba durante su preparación. Las 46 pruebas de Node se contabilizan aparte. La matriz documenta cada intento y la resolución; el cierre completo se ejecutó de nuevo sin filtros.

El respaldo completo conserva también las capturas de comprobaciones fallidas, incluyendo el ancho desbordado; Git conserva la selección de entrega. Los resultados finales y los intentos anteriores se distinguen en la matriz. No se convierten FAIL históricos en PASS. Los once archivos de `revision-entrega/` son el inventario y la propuesta históricos previos a la reducción; se mantienen intactos para su trazabilidad.

## Qué se mide

Cada escenario verifica el ancho de página, las cajas y el contenido desbordado de tarjetas, gráficos, indicador, banner y controles, y la intersección de componentes hermanos. Los gráficos deben tener SVG válido, ancho positivo, altura de 260 px y no incluir coordenadas NaN/Infinity. Se comprueba que los tooltips de las tres variables muestran el valor esperado, timestamp y zona y permanecen dentro de su tarjeta. Se guardan capturas adicionales con cada tooltip activo. Las tarjetas comprueban un cero válido; los históricos también incluyen cero. Las coordenadas de los cuatro puntos verifican distancias proporcionales para intervalos del 0 %, 1 %, 25 % y 100 % del rango.

La prueba de teclado recorre los campos y el botón, revisa el foco visible, el enlace para saltar al contenido y la navegación al histórico. Los controles se miden para comprobar una altura de interacción de al menos 44 px. A 360 px se añade un toque CDP real para reintentar la atención de una alerta. Los casos de polling esperan respuestas de los temporizadores reales de la aplicación; no se altera el reloj de producción.

## Limitaciones

- Son navegadores Windows headless con viewport CSS emulado y DPR 1; **no es una prueba en teléfono físico**. `mobile: false`; la emulación táctil a 360 px sólo comprueba esa interacción.
- Chrome y Edge comparten Chromium. No se valida Firefox, Safari, iOS, Android ni diferencias entre motores.
- No se valida la conectividad LAN, CORS del backend real, persistencia PostgreSQL o recepción desde ESP32. La evidencia previa de #92 documenta una integración real distinta.
- La geometría automática y la inspección de capturas no equivalen a una auditoría completa de accesibilidad. No se prueban lector de pantalla, zoom del sistema, teclado móvil ni los popovers de calendario nativos en dispositivos físicos.
- El caso de 5.760 puntos comprueba renderizado y consultas; no constituye un benchmark de rendimiento.
- La pantalla de 1366×768 requiere scroll vertical para ver el contenido completo; ese desplazamiento es esperado.

No se ejecutaron `git add`, commit, push ni PR.
