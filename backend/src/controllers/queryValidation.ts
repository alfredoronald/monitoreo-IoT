// Validar el calendario explícitamente: Date.parse normaliza fechas como 30 de febrero.
function parseTimestamp(value: string) {
  const match = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(Z|([+-])(\d{2}):(\d{2}))$/.exec(value);
  if (!match) return null;
  const [, yearText, monthText, dayText, hourText, minuteText, secondText, fraction = '', zone,
    , offsetHourText = '0', offsetMinuteText = '0'] = match;
  const [year, month, day, hour, minute, second, offsetHour, offsetMinute] =
    [yearText, monthText, dayText, hourText, minuteText, secondText, offsetHourText, offsetMinuteText].map(Number);
  const leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const days = [31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  if (year < 1 || month < 1 || month > 12 || day < 1 || day > days[month - 1]
    || hour > 23 || minute > 59 || second > 59 || offsetHour > 15 || offsetMinute > 59) return null;
  const seconds = Date.parse(`${yearText}-${monthText}-${dayText}T${hourText}:${minuteText}:${secondText}${zone}`);
  return Number.isFinite(seconds) ? { seconds, fraction } : null;
}

export function validateReadingFilters(variable: string, from: string, to: string): string | null {
  if (!['temperatura', 'humedad', 'co2'].includes(variable)) {
    return 'variable debe ser temperatura, humedad o co2.';
  }
  const start = parseTimestamp(from);
  if (!start) return 'desde debe ser una fecha ISO 8601 válida con hora, segundos y zona horaria.';
  const end = parseTimestamp(to);
  if (!end) return 'hasta debe ser una fecha ISO 8601 válida con hora, segundos y zona horaria.';
  // Conservar la precisión fraccionaria al comparar; Date solo conserva milisegundos.
  const width = Math.max(start.fraction.length, end.fraction.length);
  if (start.seconds > end.seconds || (start.seconds === end.seconds
    && start.fraction.padEnd(width, '0') > end.fraction.padEnd(width, '0'))) {
    return 'desde no puede ser posterior a hasta.';
  }
  return null;
}
