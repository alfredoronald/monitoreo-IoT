-- #142: conserva las calibraciones anteriores sin inventar su tamaño de muestra.
BEGIN;
ALTER TABLE public.calibraciones
    ADD COLUMN IF NOT EXISTS numero_lecturas INTEGER;
-- 15 minutos son 0.250 h; NUMERIC(6,1) los redondearía a 0.3 h.
ALTER TABLE public.calibraciones
    ALTER COLUMN acondicionamiento_horas TYPE NUMERIC(8,3);
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint
        WHERE conrelid = 'public.calibraciones'::regclass
          AND conname = 'calibraciones_numero_lecturas_positivo'
    ) THEN
        ALTER TABLE public.calibraciones
            ADD CONSTRAINT calibraciones_numero_lecturas_positivo
            CHECK (numero_lecturas IS NULL OR numero_lecturas > 0);
    END IF;
END $$;
COMMENT ON COLUMN public.calibraciones.numero_lecturas IS
    'Muestras válidas de calibración; NULL si el registro histórico no las documenta.';
COMMIT;
