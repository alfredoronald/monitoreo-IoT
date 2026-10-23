import { useEffect, useState } from 'react'
import { startReadingClock } from '../readings/readingFreshness.ts'

export function useReadingClock(timestamps: readonly string[]): number {
  const [now, setNow] = useState(() => Date.now())
  const key = timestamps.join('\n')
  useEffect(() => {
    let stop = () => {}
    function refresh() {
      stop()
      stop = startReadingClock(key ? key.split('\n') : [], setNow)
    }
    refresh()
    // Recalcular al regresar a una pestaña que el navegador pudo suspender.
    window.addEventListener('focus', refresh)
    document.addEventListener('visibilitychange', refresh)
    return () => {
      stop()
      window.removeEventListener('focus', refresh)
      document.removeEventListener('visibilitychange', refresh)
    }
  }, [key])
  return now
}
