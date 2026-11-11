export const READING_MAX_AGE_MS = 60_000

export function isReadingRecent(timestamp: string, now: number): boolean {
  return now - Date.parse(timestamp) <= READING_MAX_AGE_MS
}

// El reloj solo vuelve a renderizar la interfaz; nunca consulta la API.
export function startReadingClock(timestamps: readonly string[], onTick: (now: number) => void) {
  let timeout: ReturnType<typeof setTimeout> | undefined
  const deadlines = timestamps.map(timestamp => Date.parse(timestamp) + READING_MAX_AGE_MS + 1)
  function tick() {
    const now = Date.now()
    onTick(now)
    const next = Math.min(...deadlines.filter(deadline => deadline > now))
    if (Number.isFinite(next)) timeout = setTimeout(tick, Math.min(next - now, 2_147_483_647))
  }
  tick()
  return () => clearTimeout(timeout)
}
