import cors from 'cors';
import express from 'express';

export function createApp(corsOrigin: string) {
  const app = express();

  app.disable('x-powered-by');
  app.use(cors({ origin: corsOrigin }));
  app.use(express.json());

  app.get('/', (_request, response) => {
    response.json({
      status: 'ok',
      message: 'Servidor de monitoreo ambiental activo',
    });
  });

  return app;
}
