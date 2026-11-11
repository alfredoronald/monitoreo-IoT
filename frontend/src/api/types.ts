// Contrato de referencia: PR #103, docs/api.md, revisión 7af6e70.
export type EnvironmentalVariable = 'temperatura' | 'humedad' | 'co2'
export type AirLevel = 'bueno' | 'moderado' | 'malo'

export interface LatestReading {
  tipo_variable: EnvironmentalVariable
  valor: number
  fecha_hora: string
}

export interface HistoricalReading extends LatestReading {
  id: string
}

export interface AirState extends Omit<LatestReading, 'tipo_variable'> {
  tipo_variable: 'co2'
  nivel: AirLevel
  etiqueta: string
  recomendacion: string | null
}

export interface AirAlert {
  id: string
  lectura_id: string
  umbral_id: string
  mensaje: string
  fecha_hora: string
  atendida: boolean
}

export interface HistoryQuery {
  variable: EnvironmentalVariable
  desde: string
  hasta: string
}

export interface ApiErrorResponse {
  mensaje: string
}
