-- Ejecutar con psql -v ON_ERROR_STOP=1 y las variables indicadas en la guía.
-- Solo después de aceptar el resultado de #141. No contiene un Ro predeterminado.
BEGIN;
CREATE TEMP TABLE calibration_input (
    sensor VARCHAR(20) NOT NULL CHECK (length(trim(sensor)) > 0),
    ro_kohm NUMERIC(10,3) NOT NULL CHECK (ro_kohm > 0 AND ro_kohm <> 'NaN'::numeric),
    numero_lecturas INTEGER NOT NULL CHECK (numero_lecturas >= 50),
    co2_referencia_ppm NUMERIC(8,2) NOT NULL CHECK (co2_referencia_ppm > 0 AND co2_referencia_ppm <> 'NaN'::numeric),
    acondicionamiento_horas NUMERIC(8,3) NOT NULL CHECK (acondicionamiento_horas >= 0.25 AND acondicionamiento_horas <> 'NaN'::numeric),
    fecha_hora TIMESTAMPTZ NOT NULL,
    observaciones TEXT NOT NULL CHECK (length(trim(observaciones)) > 0)
) ON COMMIT DROP;

INSERT INTO calibration_input VALUES (
    :'sensor', :'ro_kohm', :'numero_lecturas', :'co2_referencia_ppm',
    :'acondicionamiento_horas', :'fecha_hora', :'observaciones'
);

-- Serializa el cambio de vigente, incluso si todavía no existe ninguna fila.
LOCK TABLE public.calibraciones IN SHARE ROW EXCLUSIVE MODE;
UPDATE public.calibraciones SET vigente = false
WHERE sensor = (SELECT sensor FROM calibration_input) AND vigente;

INSERT INTO public.calibraciones (
    sensor, ro_kohm, numero_lecturas, co2_referencia_ppm,
    acondicionamiento_horas, fecha_hora, vigente, observaciones
)
SELECT sensor, ro_kohm, numero_lecturas, co2_referencia_ppm,
       acondicionamiento_horas, fecha_hora, true, observaciones
FROM calibration_input
RETURNING id, sensor, ro_kohm, numero_lecturas, co2_referencia_ppm,
          acondicionamiento_horas, fecha_hora, vigente;
COMMIT;
