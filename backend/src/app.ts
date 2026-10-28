import cors from 'cors';
import { apiKeyMiddleware } from './middleware/apiKeyMiddleware.js';
import { config } from './config.js';
import express from 'express';
import { airStatusRoutes } from './routes/airStatusRoutes.js';
import { readingRoutes } from './routes/readingRoutes.js';
import { historicalReadingRoutes } from './routes/historicalReadingRoutes.js';
import { CorsOriginError, errorHandler } from './middleware/errorHandler.js';

export function createApp(corsOrigin: string, apiKey: string = config.apiKey) {
  const app = express();

  app.disable('x-powered-by');
  app.use(cors({
    origin(origin, callback) {
      // curl y otros clientes sin Origin siguen disponibles; CORS no es autenticación.
      if (origin === undefined) callback(null, false);
      else if (origin === corsOrigin && origin !== '*' && origin !== 'null') callback(null, origin);
      else callback(new CorsOriginError());
    },
    methods: ['GET', 'PATCH', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'X-API-Key'],
  }));
  app.use('/api', apiKeyMiddleware(apiKey));
  // Estas rutas rechazan cualquier cuerpo antes de que express.json lo procese.
  app.use('/api/lecturas', readingRoutes);
  app.use('/api/lecturas', historicalReadingRoutes);
  app.use(express.json());
  app.use('/api', airStatusRoutes);

  app.get('/', (_request, response) => {
    response.json({
      status: 'ok',
      message: 'Servidor de monitoreo ambiental activo',
    });
  });

  app.use(errorHandler);

  return app;
}
