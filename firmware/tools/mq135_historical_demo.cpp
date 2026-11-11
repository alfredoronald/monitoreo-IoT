#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

#include "mq135_historical_demo.h"

namespace {
using namespace mq135_historical_demo;

const char* statusName(Status status) {
  switch (status) {
    case Status::Ok: return "OK_SOLO_MATEMATICO";
    case Status::InvalidInput: return "ENTRADA_INVALIDA";
    case Status::NumericalError: return "RESULTADO_NUMERICO_INVALIDO";
    case Status::AdcOutOfRange: return "ADC_FUERA_DE_RANGO";
    case Status::AdcSaturated: return "ADC_SATURADO";
    case Status::AdcZero: return "ADC_CERO";
    case Status::SignalTooSmall: return "SENAL_INSUFICIENTE";
    case Status::ReferenceMissing: return "REFERENCIA_NO_DISPONIBLE";
    case Status::InvalidProvenance: return "PROCEDENCIA_TEMPORAL_INVALIDA";
    default: return "RECHAZADA";
  }
}

bool parsePositiveFinite(const char* text, double& value) {
  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  stream >> value;
  if (!stream || !std::isfinite(value) || value <= 0.0) return false;
  stream >> std::ws;
  return stream.eof();
}

void writeValue(std::ostream& out, const Result& result) {
  if (result.isValid() && std::isfinite(result.value)) out << result.value;
}

void writeTable(std::ostream& out, const Analysis& analysis,
                const Scenario& scenario) {
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(12);
  out << "tiempo_ms,adc,uso,vao_aproximada_no_verificada_v,q,"
         "ppm_experimentales_no_medidos,estado_q,estado_ppm,"
         "vc_nominal_asumida_v,fondo_escala_lineal_hipotetico_v,"
         "conversion_verificada,referencia_ppm_asumida\n";
  for (const Row& row : analysis.rows) {
    out << row.sample.uptimeMs << ',' << row.sample.adcRaw << ','
        << (row.usedForReference ? "REFERENCIA_DEMOSTRATIVA" : "EVALUACION")
        << ',';
    writeValue(out, row.approximateVaoV);
    out << ',';
    writeValue(out, row.q);
    out << ',';
    // Una muestra de referencia no se evalúa: campo vacío, nunca 400 forzados.
    if (!row.usedForReference) writeValue(out, row.experimentalPpm);
    out << ',' << statusName(row.q.status) << ','
        << (row.usedForReference ? "NO_EVALUADA_REFERENCIA"
                                 : statusName(row.experimentalPpm.status))
        << ',' << scenario.assumedVcV << ',' << scenario.illustrativeFullScaleV
        << ",false,"
        << mq135_experimental::ASSUMED_DEVELOPMENT_REFERENCE_PPM << '\n';
  }
}

void writeReport(std::ostream& out, const Analysis& analysis,
                 const Scenario& scenario) {
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(12);
  out << "DEMOSTRACIÓN RETROSPECTIVA, NO CONCENTRACIONES REALES DE CO₂\n"
         "Modelo provisional hasta HU-08; no completa la validación física CA-03.\n"
         "Fuente: diez lecturas históricas aportadas del monitor serie.\n"
         "Fecha/hora civil y condiciones DHT22: no disponibles.\n"
         "Montaje histórico: ESP32-WROOM-32, GPIO34, 12 bits, 11 dB;\n"
         "MQ-135 Flying-Fish nominalmente a 5 V, AO directo, sin divisor externo.\n"
         "AO/seguridad eléctrica NO verificados. Un ADC bajo no acredita seguridad.\n"
         "ID de escenario: RETROSPECTIVA_LINEAL_NO_VERIFICADA_V1\n"
         "Revisión eléctrica verificada: ninguna. No es un registro de firmware.\n"
         "Conversión HIPOTÉTICA: VAO_aprox = ADC/4095 * fondo_escala_asumido.\n"
         "No es una calibración oficial ni una tensión medida.\n"
         "Fondo de escala asumido (V): " << scenario.illustrativeFullScaleV
      << "\nVc nominal asumida, no medida (V): " << scenario.assumedVcV
      << "\nA: " << mq135_experimental::MODEL_A
      << "\nB: " << mq135_experimental::MODEL_B
      << "\nReferencia CO₂ SUPUESTA, no medida (ppm): "
      << mq135_experimental::ASSUMED_DEVELOPMENT_REFERENCE_PPM
      << "\nq = (Vc - VAO_aprox)/VAO_aprox\n"
         "ppm = 400 * pow(q/q_referencia_demostrativa, -B); A se cancela.\n"
         "Selección fijada por orden temporal, sin optimizar estabilidad/ppm.\n"
         "Muestras de referencia (tiempo_ms, ADC):\n";
  for (const Row& row : analysis.rows) {
    if (row.usedForReference) {
      out << "  " << row.sample.uptimeMs << ", " << row.sample.adcRaw << '\n';
    }
  }
  out << "Cantidad de referencia: " << analysis.referenceCount << '\n';
  if (!analysis.rows.empty()) {
    const auto& first = analysis.rows.front().sample;
    const auto& lastReference = analysis.rows[analysis.referenceCount - 1].sample;
    out << "Duración primera-última de referencia (ms): "
        << lastReference.uptimeMs - first.uptimeMs
        << "\nIntervalos observados de referencia (ms):";
    for (std::size_t i = 1; i < analysis.referenceCount; ++i) {
      out << ' ' << analysis.rows[i].sample.uptimeMs -
                       analysis.rows[i - 1].sample.uptimeMs;
    }
    out << "\nIntervalos observados del conjunto completo (ms):";
    for (std::size_t i = 1; i < analysis.rows.size(); ++i) {
      out << ' ' << analysis.rows[i].sample.uptimeMs -
                       analysis.rows[i - 1].sample.uptimeMs;
    }
    out << "\nDuración total histórica (ms): "
        << analysis.rows.back().sample.uptimeMs - first.uptimeMs << '\n';
  }
  out << "Estado de referencia: " << statusName(analysis.qReference.status)
      << "\nq_referencia_demostrativa fijo (promedio de q_i): ";
  writeValue(out, analysis.qReference);
  out << '\n';
  if (analysis.qReference.isValid()) {
    out << "Promedio ADC de referencia (códigos, no voltios): "
        << analysis.meanReferenceAdc
        << "\nDispersión relativa observada de q (max-min)/promedio: "
        << analysis.relativeQSpread << '\n';
  }
  out << "Criterio físico de aceptación: no disponible; estabilidad NO acreditada.\n"
         "La espera de firmware de 30 s no prueba precalentamiento físico.\n"
         "No hay umbral útil de señal verificado: mínimo 1 solo excluye ADC cero.\n"
         "Linealidad, ruido y utilidad de estos códigos bajos son desconocidos.\n"
         "No se transmite ni importa esta referencia al firmware.\n"
         "La evaluación usa exclusivamente las siete muestras posteriores;\n"
         "no recalcula la referencia ni mide precisión frente a CO₂ conocido.\n";
}
}  // namespace

int main(int argc, char** argv) {
  Scenario scenario{};
  if (argc != 6 || std::string(argv[1]) != "--solo-retrospectiva" ||
      !parsePositiveFinite(argv[2], scenario.illustrativeFullScaleV) ||
      !parsePositiveFinite(argv[3], scenario.assumedVcV)) {
    std::cerr << "Uso: mq135_historical_demo --solo-retrospectiva "
                 "FONDO_ESCALA_HIPOTETICO_V VC_ASUMIDA_V tabla.csv informe.txt\n"
                 "Los voltajes son SUPUESTOS, no calibración ni mediciones.\n";
    return EXIT_FAILURE;
  }
  // No sobrescribir resultados previos ni archivos existentes del repositorio.
  if (std::string(argv[4]) == argv[5] || std::ifstream(argv[4]).good() ||
      std::ifstream(argv[5]).good()) {
    std::cerr << "Seleccione dos rutas nuevas y distintas para los resultados.\n";
    return EXIT_FAILURE;
  }
  const Analysis analysis = analyze(HISTORICAL_SAMPLES, HISTORICAL_SAMPLE_COUNT,
                                   DEMONSTRATIVE_REFERENCE_COUNT, scenario);
  std::ofstream table(argv[4]);
  std::ofstream report(argv[5]);
  if (!table || !report) {
    std::cerr << "No se pudieron abrir los archivos de salida.\n";
    return EXIT_FAILURE;
  }
  writeTable(table, analysis, scenario);
  writeReport(report, analysis, scenario);
  table.flush();
  report.flush();
  if (!table || !report) {
    std::cerr << "Error al guardar los resultados.\n";
    return EXIT_FAILURE;
  }
  writeReport(std::cout, analysis, scenario);
  bool valid = analysis.qReference.isValid();
  for (const Row& row : analysis.rows) {
    valid = valid && row.q.isValid() &&
            (row.usedForReference || row.experimentalPpm.isValid());
  }
  // Éxito significa validez matemática del escenario, nunca validez física.
  return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
