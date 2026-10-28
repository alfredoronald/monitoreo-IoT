# Continuidad de 24 horas en aula — #150

Estado: **preparación; #144 abierta; ensayo físico pendiente**.
No se ha iniciado ni acreditado una ventana de 24 horas.

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
backend, red y sensores durante 24 h; desactivar suspensión automática de la PC.
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

La ventana es exactamente de 86.400 s; no se ofrecen ventanas abreviadas como evidencia.
A 15 s: 5.760 esperadas y mínimo 5.645 por variable; 17.280 y mínimo 16.935 en total.
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
| Temperatura | Pendiente | Pendiente | — | — | — | Pendiente |
| Humedad | Pendiente | Pendiente | — | — | — | Pendiente |
| CO2 | Pendiente | Pendiente | — | — | — | Pendiente |

Prueba de cálculos: `pnpm exec tsx --test tests/continuityRun.test.ts`.
Los tests de escritorio no reemplazan las 24 horas en el aula.
