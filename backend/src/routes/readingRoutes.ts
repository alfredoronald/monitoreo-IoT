import { Router } from 'express';
import { getLatestReadingsHandler } from '../controllers/readingController.js';

export const readingRoutes = Router();

readingRoutes.get('/ultima', getLatestReadingsHandler);
