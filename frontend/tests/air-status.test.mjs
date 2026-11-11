import assert from 'node:assert/strict'
import test from 'node:test'
import { createElement } from 'react'
import { renderToStaticMarkup } from 'react-dom/server'
import { createServer } from 'vite'

test('Indicador: conserva los tres niveles, textos y colores; una clasificación antigua no es actual', async t => {
  const server = await createServer({ configFile: false, logLevel: 'silent', server: { middlewareMode: true, hmr: false } })
  t.after(() => server.close())
  const { AirStatusIndicator } = await server.ssrLoadModule('/src/components/AirStatusIndicator.tsx')
  const fecha_hora = '2026-10-18T16:00:00.000Z'
  const render = props => renderToStaticMarkup(createElement(AirStatusIndicator, {
    loading: false, error: null, now: Date.parse(fecha_hora) + 60_000, ...props,
  }))
  for (const nivel of ['bueno', 'moderado', 'malo']) {
    const airState = { tipo_variable: 'co2', valor: 0, fecha_hora, nivel, etiqueta: 'Etiqueta del servidor', recomendacion: 'Recomendación del servidor' }
    const fresh = render({ airState })
    assert.match(fresh, /Nivel actual/)
    assert.doesNotMatch(fresh, /Sin datos recientes/)
    const stale = render({ airState, now: Date.parse(fecha_hora) + 60_001 })
    for (const html of [fresh, stale]) {
      assert.ok(html.includes(`air-status-${nivel}`))
      assert.match(html, /Etiqueta del servidor/)
      assert.match(html, /Recomendación del servidor/)
      assert.match(html, /0 ppm/)
      assert.ok(html.includes(fecha_hora))
    }
    assert.match(stale, /Última clasificación conocida/)
    assert.match(stale, /Última lectura conocida/)
    assert.match(stale, /Sin datos recientes/)
    assert.doesNotMatch(stale, /Nivel actual/)
    const failed = render({ airState, error: 'Error de API' })
    assert.match(failed, /Última clasificación conocida/)
    assert.match(failed, /Se conserva el último estado/)
  }
  assert.match(render({ airState: null, loading: true }), /Cargando estado del aire/)
  assert.match(render({ airState: null }), /Sin datos para clasificar/)
  const error = render({ airState: null, error: 'Error de API' })
  assert.match(error, /role="alert"/)
  assert.match(error, /error de consulta/)
  assert.doesNotMatch(error, /Se conserva|Sin datos para clasificar/)
})
