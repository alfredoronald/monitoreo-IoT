import { Router } from 'express';
import {
  getAirStatusHandler,
  getAlertsHandler,
  patchAlertHandler,
} from '../controllers/airStatusController.js';

export const airStatusRoutes = Router();

airStatusRoutes.get('/estado-aire', getAirStatusHandler);
airStatusRoutes.get('/alertas', getAlertsHandler);
airStatusRoutes.patch('/alertas/:id', patchAlertHandler);
