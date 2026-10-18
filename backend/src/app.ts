import cors from 'cors';
import express from 'express';
import { airStatusRoutes } from './routes/airStatusRoutes.js';

export function createApp(corsOrigin: string) {
  const app = express();

  app.disable('x-powered-by');
  app.use(cors({ origin: corsOrigin }));
  app.use(express.json());
  app.use('/api', airStatusRoutes);

  app.get('/', (_request, response) => {
    response.json({
      status: 'ok',
      message: 'Servidor de monitoreo ambiental activo',
    });
  });

  app.use((error: unknown, _request: express.Request, response: express.Response, next: express.NextFunction) => {
    if (error instanceof SyntaxError && 'body' in error) {
      response.status(400).json({ mensaje: 'El cuerpo no contiene JSON válido.' });
      return;
    }
    next(error);
  });

  return app;
}
