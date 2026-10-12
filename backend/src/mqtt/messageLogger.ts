import { appendFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

export const defaultMessageLogPath = fileURLToPath(
  new URL('../../logs/mqtt-messages.jsonl', import.meta.url),
);

export function redactLogText(text: string): string {
  let sanitized = text;
  const secrets = Object.entries(process.env)
    .filter(([name, value]) => /(?:PASS(?:WORD)?|SECRET|TOKEN|API_KEY)$/i.test(name) && value)
    .map(([, value]) => value!)
    .sort((left, right) => right.length - left.length);
  for (const secret of secrets) sanitized = sanitized.split(secret).join('[REDACTADO]');

  // También ocultar credenciales identificadas en un mensaje o error externo.
  return sanitized
    .replace(/((?:[\w-]*(?:password|passwd|pass|secret|token)|api[_-]?key)["']?\s*[:=]\s*)(?:"[^"\r\n]*"|'[^'\r\n]*'|[^\s,;}]+)/gi, '$1[REDACTADO]')
    .replace(/(\w+:\/\/[^\s/@:]+:)[^\s/@]+@/g, '$1[REDACTADO]@');
}

export function createMessageLogger(filePath = defaultMessageLogPath) {
  return (event: 'received' | 'discarded', topic: string, content: string, reason?: string) => {
    const entry = {
      timestamp: new Date().toISOString(),
      event,
      topic: redactLogText(topic),
      content: redactLogText(content),
      ...(reason === undefined ? {} : { reason: redactLogText(reason) }),
    };
    const line = JSON.stringify(entry);
    console.log(`[MQTT] ${event === 'received' ? 'Recibido' : 'Descartado'}: ${line}`);
    try {
      mkdirSync(dirname(filePath), { recursive: true });
      appendFileSync(filePath, `${line}\n`, 'utf8');
    } catch {
      // Un fallo de escritura no debe impedir validar ni guardar lecturas.
      console.error('[MQTT] No se pudo escribir el log de mensajes; revisa permisos y espacio disponible');
    }
  };
}
