import type { EnvironmentalVariable } from './api/types.ts'

export const environmentalVariables = [
  { id: 'temperature', variable: 'temperatura', name: 'Temperatura', unit: '°C', description: 'Temperatura del aula' },
  { id: 'humidity', variable: 'humedad', name: 'Humedad', unit: '% HR', description: 'Humedad relativa' },
  { id: 'co2', variable: 'co2', name: 'CO₂ estimado', unit: 'ppm', description: 'Estimación del MQ-135' },
] as const satisfies readonly { id: string; variable: EnvironmentalVariable; name: string; unit: string; description: string }[]
