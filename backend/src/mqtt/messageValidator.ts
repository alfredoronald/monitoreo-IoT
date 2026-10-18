export type ReadingVariable = 'temperatura' | 'humedad' | 'co2';

export interface Reading {
  tipo_variable: ReadingVariable;
  valor: number;
  fecha_hora: string;
}

export type MessageValidationResult =
  | { valid: true; reading: Reading }
  | { valid: false; reason: string };

const readingTopics = new Map<string, ReadingVariable>([
  ['ambiente/temperatura', 'temperatura'],
  ['ambiente/humedad', 'humedad'],
  ['ambiente/co2', 'co2'],
]);

const numericPattern = /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/;

export function validateMessage(topic: string, content: string): MessageValidationResult {
  const variable = readingTopics.get(topic);

  if (!variable) {
    return {
      valid: false,
      reason: topic === 'ambiente/status'
        ? 'El mensaje de estado no es una lectura'
        : 'Topic de lectura desconocido',
    };
  }

  const normalizedContent = content.trim();
  if (!numericPattern.test(normalizedContent)) {
    return { valid: false, reason: 'El contenido no es numérico' };
  }

  const value = Number(normalizedContent);
  if (!Number.isFinite(value)) {
    return { valid: false, reason: 'El contenido no es un número finito' };
  }

  return {
    valid: true,
    reading: {
      tipo_variable: variable,
      valor: value,
      fecha_hora: new Date().toISOString(),
    },
  };
}
