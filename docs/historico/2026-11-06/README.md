# Historico compartido: sesion del 6 de noviembre de 2026

## Contenido y procedencia

- lecturas.csv: **8,465 filas** de public.lecturas (temperatura 2,825, humedad 2,821, CO2 estimado 2,819).
- Sesion del 06/11 07:41 al 06/11 20:02, America/La_Paz: unas **12.3 h**, con el circuito ESP32 conectado y 6 cortes de mas de 60 s en temperatura.
- resumen.json: conteos, minimos, promedios, maximos y huecos mayores a 60 s por variable. No son 24 h continuas.
- esquema-referencia.sql: esquema versionado del proyecto (database/esquema.sql), para quien aun no lo tenga instalado.
- importar-lecturas.sql: carga transaccional e idempotente de lecturas.csv; no contiene credenciales. Es el mismo importador del paquete del 28-29/10, con la cantidad de filas de esta sesion.
- manifest-sha256.json: hashes de los archivos.

El CSV se genero consultando la API del proyecto (GET /api/lecturas), no la base de datos directamente. El CSV preserva el ID de origen y las fechas; el importador genera IDs locales nuevos. No se reescriben timestamps para aparentar datos actuales.

## Advertencia sobre el CO2 de esta sesion

Los valores de CO2 estimado de esta sesion son **anormalmente bajos** (minimo 7.5 ppm, promedio 13.04 ppm, maximo 235.8 ppm): el MQ-135 mostro una senal base baja respecto a la calibracion del 28/10. No tomar estas filas como CO2 calibrado ni como validacion fisica aprobada. Sirven para ver la continuidad, los cortes y el comportamiento del dashboard. Temperatura y humedad no presentan esa anomalia.

## Importacion en PostgreSQL 15 o superior

1. Extraer TODO el ZIP a una carpeta y abrir una terminal en esa carpeta. Verificar manifest-sha256.json antes de importar. No versionar contrasenas.
2. Tener PostgreSQL y la BD del proyecto instalados (README e infra/postgres del repositorio). Detener temporalmente la ingesta del backend en la BD de destino. La BD existente se conserva.
3. Ejecutar, adaptando puerto y usuario locales:

```bash
psql -h localhost -p 5432 -U app -d monitoreo_ambiental -W -v ON_ERROR_STOP=1 -f importar-lecturas.sql
```

4. Introducir la contrasena local cuando la pida. No se incluye ninguna en el paquete.
5. Consultar el dashboard con el rango personalizado 06/11/2026 07:00 a 06/11/2026 21:00 (hora de Bolivia). Importar el CSV no arranca servicios ni crea lecturas recientes.

Si las lecturas ya estan en tu base (por ejemplo, en la computadora donde se capturaron), el importador no duplica nada: `filas_insertadas` sera 0 y `filas_del_paquete_presentes` igual a 8,465.
