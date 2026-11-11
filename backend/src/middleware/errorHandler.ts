import type { ErrorRequestHandler } from 'express';

export class CorsOriginError extends Error {}

// Los detalles del error nunca forman parte de la respuesta pública.
export const errorHandler: ErrorRequestHandler = (error: unknown, _request, response, next) => {
  if (response.headersSent) {
    next(error);
    return;
  }
  if (error instanceof CorsOriginError) {
    response.status(403).json({ mensaje: 'Origen no permitido por CORS.' });
    return;
  }
  const type = error !== null && typeof error === 'object' && 'type' in error ? error.type : undefined;
  if (type === 'entity.parse.failed') {
    response.status(400).json({ mensaje: 'El cuerpo no contiene JSON válido.' });
    return;
  }
  if (type === 'entity.too.large') {
    response.status(413).json({ mensaje: 'El cuerpo de la solicitud supera el tamaño permitido.' });
    return;
  }
  response.status(500).json({ mensaje: 'No se pudo completar la solicitud.' });
};
