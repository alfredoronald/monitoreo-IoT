import assert from 'node:assert/strict'
import test from 'node:test'
import { createElement } from 'react'
import { renderToStaticMarkup } from 'react-dom/server'
import { createServer } from 'vite'
import { environmentalVariables } from '../src/environmentalVariables.ts'

test('Componentes históricos: estados accesibles y tooltip sin perder valor ni fecha original', async t => {
  const server = await createServer({ configFile: false, logLevel: 'silent', server: { middlewareMode: true, hmr: false } })
  t.after(() => server.close())
  const { HistoricalChart } = await server.ssrLoadModule('/src/components/history/HistoricalChart.tsx')
  const { HistoricalTooltip } = await server.ssrLoadModule('/src/components/history/HistoricalTooltip.tsx')
  const range = { desde: '2026-10-15T12:00:00.000Z', hasta: '2026-10-15T13:00:00.000Z' }
  for (const variable of environmentalVariables) {
    const render = series => renderToStaticMarkup(createElement(HistoricalChart, { variable, range, series }))
    const loading = render({ readings: [], loading: true, error: null })
    assert.match(loading, /aria-busy="true"/)
    assert.match(loading, /role="status"/)
    assert.match(loading, /Cargando histórico/)
    const empty = render({ readings: [], loading: false, error: null })
    assert.match(empty, /No hay lecturas en este rango/)
    assert.doesNotMatch(empty, /history-chart"/)
    const error = render({ readings: [], loading: false, error: 'No se pudo cargar el histórico.' })
    assert.match(error, /role="alert"/)
    assert.doesNotMatch(error, /No hay lecturas/)
    const reading = { id: '9007199254740993', tipo_variable: variable.variable, valor: 0, fecha_hora: '2026-10-15T12:00:15.123Z' }
    const tooltip = renderToStaticMarkup(createElement(HistoricalTooltip, { reading, unit: variable.unit }))
    assert.ok(tooltip.includes(`0 ${variable.unit}`))
    assert.match(tooltip, /dateTime="2026-10-15T12:00:15.123Z"/i)
    assert.match(tooltip, /2026/)
    assert.match(tooltip, /123/)
    assert.equal(renderToStaticMarkup(createElement(HistoricalTooltip, { unit: variable.unit })), '')
  }
})
