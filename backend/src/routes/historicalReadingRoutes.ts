import { Router } from 'express';
import { getHistoricalReadingsHandler } from '../controllers/historicalReadingController.js';

export const historicalReadingRoutes = Router();

historicalReadingRoutes.get('/', getHistoricalReadingsHandler);
