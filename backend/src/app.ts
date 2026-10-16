import cors from 'cors';
import express from 'express';
import { readingRoutes } from './routes/readingRoutes.js';
import { historicalReadingRoutes } from './routes/historicalReadingRoutes.js';

export function createApp(corsOrigin: string) {
  const app = express();

  app.disable('x-powered-by');
  app.use(cors({ origin: corsOrigin }));
  // Estas rutas rechazan cualquier cuerpo antes de que express.json lo procese.
  app.use('/api/lecturas', readingRoutes);
  app.use('/api/lecturas', historicalReadingRoutes);
  app.use(express.json());

  app.get('/', (_request, response) => {
    response.json({
      status: 'ok',
      message: 'Servidor de monitoreo ambiental activo',
    });
  });

  return app;
}
