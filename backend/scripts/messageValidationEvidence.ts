import { validateMessage } from '../src/mqtt/messageValidator.js';

console.log('Evidencia de validación de mensajes MQTT - #20');
console.log('Ejecución directa del módulo, sin broker ni inserciones en PostgreSQL.\n');

for (const [topic, content] of [
  ['ambiente/temperatura', '24.5'],
  ['ambiente/humedad', 'abc'],
  ['ambiente/status', '1'],
  ['ambiente/presion', '1013'],
]) {
  console.log(`Topic: ${topic} | Contenido: ${content}`);
  const result = validateMessage(topic, content);
  console.log(result.valid
    ? `Lectura válida:\n${JSON.stringify(result.reading, null, 2)}`
    : `Descartado: ${result.reason}`);
  console.log();
}
