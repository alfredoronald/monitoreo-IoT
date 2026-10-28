-- #152: ampliar la tabla existente sin duplicarla ni alterar las lecturas.
ALTER TABLE pruebas_transmision ADD COLUMN IF NOT EXISTS ejecucion_clave TEXT;
ALTER TABLE pruebas_transmision ADD COLUMN IF NOT EXISTS metricas JSONB;
CREATE UNIQUE INDEX IF NOT EXISTS uq_pruebas_transmision_ejecucion
    ON pruebas_transmision (ejecucion_clave);
