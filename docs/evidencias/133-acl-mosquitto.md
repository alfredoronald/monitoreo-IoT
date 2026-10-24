# Evidencia de #133: ACL y rotación de contraseñas MQTT

## Alcance y entorno

- Tarea: #133. Historia padre: #137.
- Rama: `chore/133-mosquitto-acl-hardening`.
- Base y destino de la PR: `feature/backend-mqtt-api`.
- Entorno local: Windows, Mosquitto 2.1.2, Node.js 24.14.0 y PlatformIO 6.2.0.
- Verificación sin circuito conectado. Las pruebas físicas anteriores del
  proyecto no sustituyen la conexión con las credenciales recién rotadas.

## Política versionada

El archivo [acl](../../infra/mosquitto/acl) ya tenía los permisos mínimos.
Se conservaron y se documentaron en español la denegación por defecto y el
filtrado de entrega de mensajes:

```conf
user esp32
topic write ambiente/#

user backend
topic read ambiente/#
```

La configuración real del broker usa `allow_anonymous false`, `password_file`
y `acl_file`. No se añadieron permisos globales ni de lectura/escritura conjunta.

## Rotación local realizada

- Se generaron dos contraseñas aleatorias distintas de **32 caracteres**.
- Se actualizaron con `mosquitto_passwd`, sin recrear el archivo con `-c`.
- Se eligió SHA-512 PBKDF2 para mantener compatibilidad con Mosquitto 2.0 y 2.1.
- El archivo privado `passwd` contiene únicamente los usuarios `esp32` y `backend`.
- Se sincronizaron `MQTT_USER`/`MQTT_PASS` en `backend/.env` y
  `mqtt_user`/`mqtt_password` en `firmware/include/config.h`.
- Se verificó que las claves nuevas son distintas, cumplen la longitud mínima
  y coinciden entre el archivo privado del cliente y la autenticación del broker.
- No se introdujo `secrets.h`.

Los valores y hashes no forman parte de esta evidencia. El procedimiento para
repetir la rotación está en el [README](../../README.md#rotar-las-contraseñas-mqtt-133).

## Resultados MQTT con broker real

Se inició Mosquitto con `infra/mosquitto/mosquitto.conf` desde la raíz, en el
puerto 1883. Los clientes de prueba usaron MQTT 5 y QoS 1 para observar el
resultado de las publicaciones, además de una conexión MQTT 3.1.1 para comprobar
el protocolo utilizado por PubSubClient en el firmware.

| Comprobación | Resultado observado |
|---|---|
| `esp32` conecta con su clave nueva, MQTT 3.1.1 | Conexión aceptada |
| `esp32` publica en `ambiente/status` | Publicación aceptada y recibida por `backend` |
| `esp32` publica en `ambiente/issue133/nested` | Publicación aceptada y recibida por `backend` |
| `esp32` se suscribe a `ambiente/#` mientras publica | No recibe los mensajes que sí recibe `backend` |
| `backend` publica en `ambiente/status` | PUBACK 135: no autorizado |
| `backend` publica en `fuera/issue133` y `$SYS/issue133` | PUBACK 135 en ambos casos |
| `esp32` publica en `fuera/issue133` y `$SYS/issue133` | PUBACK 135 en ambos casos |
| Acceso anónimo | CONNACK 135: no autorizado |
| Usuario desconocido con una clave válida de otro usuario | CONNACK 135 |
| `backend` con contraseña incorrecta | CONNACK 135 |
| `backend` con su contraseña inmediatamente anterior | CONNACK 135 |
| `esp32` con su contraseña inmediatamente anterior | CONNACK 135 |

### Lectura fuera del prefijo permitido

Para que una prueba negativa no dependa de la ausencia de publicaciones, se usó
un segundo broker aislado en `127.0.0.1:18884`, con persistencia temporal:

1. Solo en ese broker aislado se cargaron dos mensajes retenidos conocidos:
   `fuera/issue133` y `ambiente/issue133/retained`, mediante un ACL de preparación.
2. Se guardó la persistencia y se reinició el broker aislado usando el **ACL
   versionado del repositorio** y el archivo de contraseñas actualizado.
3. Cada usuario solicitó los filtros `#`, `fuera/#` y `$SYS/#`.
4. `backend` recibió el retenido de `ambiente/` y no recibió el retenido externo.
   `esp32` no recibió ninguno. Ninguno recibió mensajes de `$SYS/`.

El permiso amplio de preparación solo existió en un archivo temporal del broker
aislado. El ACL del repositorio y el broker de 1883 mantuvieron su política mínima.
Los procesos y archivos temporales del broker aislado se eliminaron al terminar.

## Backend y compilaciones

| Verificación | Resultado |
|---|---|
| `pnpm typecheck`, desde `backend/` | Correcto |
| `pnpm build`, desde `backend/` | Correcto |
| `pnpm test`, desde `backend/` | 101 pruebas aprobadas, 0 fallidas |
| Backend real con su `.env` local | Arranca con PostgreSQL, conecta MQTT, se suscribe y registra el mensaje de prueba |
| Reinicio de Mosquitto con backend activo | Backend detecta desconexión, reconecta, se resuscribe y recibe otro mensaje |
| `pio run -e esp32dev`, desde `firmware/` | Correcto |
| Binario local generado | Se verificó que incorpora la credencial nueva, sin imprimirla |

En esta computadora se ejecutó PlatformIO mediante
`%USERPROFILE%\.platformio\penv\Scripts\platformio.exe` porque `pio` no estaba
en el PATH de la consola. El binario está en
`firmware/.pio/build/esp32dev/firmware.bin`, ignorado por Git.

Los mensajes enviados al backend fueron marcadores sintéticos en topics de estado
o prueba, no mediciones de sensores. No se insertaron lecturas ambientales de
prueba mediante estas verificaciones. Los servicios iniciados para la prueba
se detuvieron al finalizar.

## Pendiente para la presentación: ESP32 físico

La placa conserva su firmware y contraseña anteriores hasta volver a cargarla.
Conectar el circuito y, desde la terminal de PlatformIO en `firmware/`, ejecutar:

```powershell
pio run -e esp32dev -t upload
pio device monitor --baud 115200
```

Antes de la carga, comprobar que el host MQTT y la red WiFi del `config.h` local
corresponden a la red de la presentación. Arrancar Mosquitto y el backend según
el README. Después de cargar, pulsar EN/RESET y verificar:

- [ ] El ESP32 conecta a WiFi y MQTT con las credenciales nuevas.
- [ ] Publica lecturas en `ambiente/#` y el backend las recibe y guarda.

**Estado del criterio «ESP32 y backend siguen conectando con las credenciales
nuevas»: backend verificado; conexión del ESP32 físico pendiente.** La conexión
del cliente de prueba como `esp32` y la compilación no acreditan esa prueba física.

## Exclusión de secretos

`git check-ignore` confirma las exclusiones de `backend/.env`,
`firmware/include/config.h`, `infra/mosquitto/passwd` y el binario de `.pio/`.
Los tres archivos privados no están versionados. Los cambios entregables son
el ACL, el README y esta evidencia.
