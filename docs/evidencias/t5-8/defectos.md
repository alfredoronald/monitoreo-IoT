# Defectos reproducidos — T-5.8

Tras la reducción del 22/10/2026, las seis capturas originales enlazadas aquí para D87-01/D87-02 (dos antes y cuatro después) siguen disponibles directamente en Git. Los resultados JSON permanecen intactos: 146 PASS y 0 FAIL de cierre, y diez FAIL históricos. Las capturas de los ocho fallos de preparación de la herramienta se conservan en el [respaldo local](archivo-local.md#capturas-archivadas), que no está disponible automáticamente para otros colaboradores. Los 209 originales, incluidas las versiones previas de estos documentos, fueron respaldados y verificados por SHA-256 antes de retirar archivos.

## D87-01 — Valores numéricos largos provocan scroll horizontal

- Detectado antes de corregir, en Chrome, viewport CSS 360 × 800.
- Reproducción sobre el CSS de la revisión base, sin las correcciones: `npm.cmd run test:browser:responsive -- --browser=chrome --viewports=360x800 --breakpoints=no --phase=reproduccion`. El intento histórico se guardó con `--phase=antes`; no repetir ese nombre si se desea conservar su evidencia. Con el CSS corregido, este mismo escenario debe aprobar.
- Escenario: `17-valores-numericos-largos`. La API controlada devuelve el número finito `123456789012345.6` en las tarjetas. No cambia el contrato API.
- Esperado: el valor completo y su unidad caben en la tarjeta, sin recortarse ni generar desplazamiento horizontal.
- Observado: `document.documentElement.scrollWidth = 480`, `innerWidth = 360`.
- Severidad: media. El dato es extremo y sintético; revela que la tarjeta carece de ajuste para valores largos.
- Evidencia antes: [captura](capturas/chrome/360x800/antes/17-valores-numericos-largos.png), [registro original](resultados-chrome-antes.json).
- Corrección: `.metric-value` permite varias líneas; su primer `span` tiene `min-width: 0`, `max-width: 100%` y `overflow-wrap: anywhere`. No se trunca ni redondea adicionalmente el número; la unidad puede pasar a la siguiente línea.
- Repetición: mismo fixture y escenario con CSS corregido, en Chrome y Edge, a 360, 768 y 1366 px. Los registros de cierre contienen la geometría y el resultado individual.
- Evidencia después: [Chrome 360 px](capturas/chrome/360x800/cierre/17-valores-numericos-largos.png), [Edge 360 px](capturas/edge/360x800/cierre/17-valores-numericos-largos.png).
- Estado: corregido y repetido. La regresión completa se detalla en [matriz-pruebas.md](matriz-pruebas.md).

## D87-02 — Textos del indicador de aire sin espacios desbordan la pantalla

- Detectado antes de corregir, en Chrome, viewport CSS 360 × 800.
- Mismo comando de reproducción; escenario `18-textos-aire-sin-espacios`.
- La respuesta controlada incluye etiqueta `CALIDAD` repetida 25 veces y recomendación `VENTILAR` repetida 40 veces. Son strings válidos del contrato.
- Esperado: partir texto dentro del panel y conservar su contenido completo.
- Observado: ancho de página de 2870 px para viewport de 360 px.
- Severidad: media. Una etiqueta o recomendación sin espacios puede romper todo el dashboard.
- Evidencia antes: [captura](capturas/chrome/360x800/antes/18-textos-aire-sin-espacios.png), [registro original](resultados-chrome-antes.json).
- Corrección: `.air-status-tag` limita su ancho al panel y permite partir cadenas; `.air-status-recommendation` también permite `overflow-wrap: anywhere`. Se conservan completos los textos recibidos.
- Repetición: mismo fixture y escenario con CSS corregido, en Chrome y Edge, a 360, 768 y 1366 px. Los registros de cierre contienen la geometría y el resultado individual.
- Evidencia después: [Chrome 360 px](capturas/chrome/360x800/cierre/18-textos-aire-sin-espacios.png), [Edge 360 px](capturas/edge/360x800/cierre/18-textos-aire-sin-espacios.png).
- Estado: corregido y repetido. La regresión completa se detalla en [matriz-pruebas.md](matriz-pruebas.md).

## Incidencias de la herramienta, separadas de los defectos del producto

La primera ejecución de diagnóstico también detectó dos errores de la prueba nueva: suponía exactamente tres solicitudes iniciales aunque React StrictMode puede emitir seis, y contenía un selector mal escapado en `Runtime.evaluate`. Se corrigieron en la prueba. Sus FAIL se conservan en el registro `antes`; no se contabilizan como defectos del dashboard. La conexión CDP inicial dentro del sandbox agotó su tiempo de espera; se ejecutó con los permisos autorizados. No se alteraron los scripts de #85/#86.

Los intentos `integracion-inicial` y `regresion` también conservan fallos de activación/expectativa del tooltip: el primer intento colocaba el puntero en el borde exacto del primer punto; el segundo lo desplazó dos píxeles y, con intervalos cercanos, seleccionó la muestra vecina en vez del cero esperado. La prueba definitiva usa el tercer punto interior, comprueba su valor y sus distancias temporales. La comprobación específica `tooltips` aprobó las seis combinaciones sin cambios adicionales del producto. En el primer intento integral, el escenario global de excepciones etiquetaba el viewport con 1366 px aunque seguía activo el último breakpoint de 901 px; el cierre restablece el viewport real mediante CDP. Estas incidencias no se cuentan como defectos CSS.
