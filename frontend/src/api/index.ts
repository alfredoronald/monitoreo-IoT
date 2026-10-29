import { createApiClient } from './client.ts'

// Instancia para el navegador. Sin URL configurada no se envía una petición
// accidental al servidor de Vite ni se usa un host fijado en el código.
const baseUrl = import.meta.env.VITE_API_URL?.trim()
if (!baseUrl) throw new Error('Configura VITE_API_URL en frontend/.env y reinicia Vite.')
const apiKey = import.meta.env.VITE_API_KEY?.trim() ?? ''

export const api = createApiClient(baseUrl, apiKey)
export { ApiError, createApiClient } from './client.ts'
export type { AirAlert, AirLevel, AirState, ApiErrorResponse, EnvironmentalVariable, HistoricalReading, HistoryQuery, LatestReading } from './types.ts'
