# Continuidad de 5 minutos en aula — #150

Estado: **cinco minutos físicos completados y aprobados el 29/10/2026; listo para revisión**.
Alcance reducido de 24 horas a 5 minutos por solicitud del responsable.

## Sesión física del 29/10/2026

Ventana UTC: **15:27:16.551 → 15:32:16.551**, exactamente 300 s.
El monitor cerró a las 15:32:17.174 UTC, contando únicamente dentro de la ventana.
**60/60 lecturas guardadas (100%); ninguna incidencia registrada.**
La verificación SQL independiente confirmó esos conteos y tres filas de resumen
en `pruebas_transmision`, con duración de 5 minutos y pérdida calculada de 0%.
Ver `verificar-continuidad.sql` y `verificacion-postgres.json` (incluye las 60 filas).
El mayor intervalo observado entre filas de una misma variable fue 23,417 s:
el intervalo configurado es 15 s, pero el transporte presenta variación temporal.
No se infiere una cadencia perfecta a partir del conteo total. Las 15 lecturas
anteriores a la sesión permanecen intactas.

El usuario confirmó explícitamente que el circuito está en el aula real.
Firmware `7a0ac17` cargado en COM5, publicación cada 15 s conforme a #144
(cerrada), backend `ab382e3`, Mosquitto 2.1.2 y PostgreSQL real. La red de
2,4 GHz se configuró privadamente tras el cambio de hotspot.
Los cortes de #149 finalizaron antes de esta ventana. Durante los cinco minutos
no se provocan fallos, no se recarga el firmware ni se publican mensajes sintéticos.

El archivo `continuidad-fisica-2026-10-29.json` conserva inicio y fin planificado,
checkpoint, conteos e incidencias. La confirmación de ubicación es del operador,
no una verificación remota independiente. `CO2` es la estimación del MQ-135 con
Ro experimental de 49,486 kΩ; sus valores anormalmente bajos en esta sesión se
conservan sin corregirlos. Este ensayo verifica continuidad de transporte y
persistencia, no exactitud de CO2 ni una calibración nueva.

1. Integrar el intervalo final de #144 y desplegar versiones compatibles.
2. Llevar el circuito al aula y comprobar las tres variables físicas recientes.
   No publicar mensajes sintéticos ni ejecutar las fallas de #149 en esta ventana.
3. Desde `backend/`, configurar las credenciales habituales en privado y ejecutar:

```powershell
$env:CONTINUITY_AULA_CONFIRMED='yes'
$env:SAMPLING_INTERVAL_FINAL_CONFIRMED='yes'
pnpm exec tsx scripts/continuityRun.ts start logs/continuidad-150.json 15
```

Reemplazar `15` por el intervalo realmente configurado. Mantener PC, broker,
backend, red y sensores durante 5 minutos; desactivar suspensión automática de la PC.
El script mantiene checkpoints cada minuto. `Ctrl+C` guarda un resultado incompleto;
reanudar con `pnpm exec tsx scripts/continuityRun.ts resume logs/continuidad-150.json`.
Reanudar no borra incidencias ni reinicia el reloj; documentar la ausencia del monitor.

El conteo se realiza en PostgreSQL mediante:

```sql
SELECT tipo_variable, count(*)
FROM lecturas
WHERE id > :baseline_id AND fecha_hora >= :inicio_utc AND fecha_hora < :fin_utc
GROUP BY tipo_variable;
```

La ventana es exactamente de 300 s y solo acredita continuidad durante 5 minutos.
A 15 s: 20 esperadas y mínimo 20 por variable; 60 y mínimo 60 en total.
Con muestras pequeñas, el umbral del 98% exige todas las lecturas esperadas.
Un conteo superior a lo esperado se marca como desvío para investigar duplicados o
publicaciones ajenas; no se usa para ocultar pérdidas. Los límites deben corresponder
a la cadencia real (diferencias de fase de un mensaje deben explicarse).

Al completar, inserta tres filas identificadas por ejecución en `pruebas_transmision`,
con transacción y bloqueo para evitar duplicación. `enviados` significa esperadas
teóricas; `recibidos` es el mínimo confirmado por las filas persistidas, NO un conteo
independiente MQTT. Ambas definiciones se guardan en `observaciones`.
No borra lecturas físicas. Si falla la persistencia del resumen, consultar el checkpoint
y completar la carga en #152. Revisar logs e incidencias además del porcentaje: el
conteo por sí solo no acredita disponibilidad continua del circuito.

| Variable | Inicio UTC | Fin UTC | Esperadas | Guardadas | Porcentaje | Resultado |
|---|---|---|---:|---:|---:|---|
| Temperatura | 2026-10-29T15:27:16.551Z | 2026-10-29T15:32:16.551Z | 20 | 20 | 100% | PASS |
| Humedad | 2026-10-29T15:27:16.551Z | 2026-10-29T15:32:16.551Z | 20 | 20 | 100% | PASS |
| CO2 | 2026-10-29T15:27:16.551Z | 2026-10-29T15:32:16.551Z | 20 | 20 | 100% | PASS |

Prueba de cálculos: `pnpm exec tsx --test tests/continuityRun.test.ts`.
Los tests de escritorio no reemplazan los 5 minutos físicos en el aula.
