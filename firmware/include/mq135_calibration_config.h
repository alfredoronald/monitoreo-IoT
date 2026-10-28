#ifndef MONITOREO_IOT_MQ135_CALIBRATION_CONFIG_H
#define MONITOREO_IOT_MQ135_CALIBRATION_CONFIG_H

#include "mq135_calibration.h"
#include <cstring>

// Configuracion explicita por build_flags o config.h privado (incluido antes).
// Estimacion autorizada para #140; no acredita medicion ni topologia real.
// Desactivar para exigir exclusivamente una carga fisicamente verificada.
#ifndef MQ135_EXPERIMENTAL_RS
#define MQ135_EXPERIMENTAL_RS 1
#endif
#if MQ135_EXPERIMENTAL_RS != 0 && MQ135_EXPERIMENTAL_RS != 1
#error "MQ135_EXPERIMENTAL_RS debe ser 0 o 1"
#endif
// Cero significa SIN CONFIGURAR en modo estricto.
#ifndef MQ135_EFFECTIVE_RL_OHMS
#if MQ135_EXPERIMENTAL_RS
#define MQ135_EFFECTIVE_RL_OHMS 6666.67
#else
#define MQ135_EFFECTIVE_RL_OHMS 0.0
#endif
#endif
#ifndef MQ135_RL_TOPOLOGY_VERIFIED
#define MQ135_RL_TOPOLOGY_VERIFIED 0
#endif
#if MQ135_RL_TOPOLOGY_VERIFIED != 0 && MQ135_RL_TOPOLOGY_VERIFIED != 1
#error "MQ135_RL_TOPOLOGY_VERIFIED debe ser 0 o 1"
#endif
#ifndef MQ135_RL_CIRCUIT_REVISION
#if MQ135_EXPERIMENTAL_RS
#define MQ135_RL_CIRCUIT_REVISION 3
#else
#define MQ135_RL_CIRCUIT_REVISION 0
#endif
#endif
#ifndef MQ135_RL_EVIDENCE_ID
#if MQ135_EXPERIMENTAL_RS
#define MQ135_RL_EVIDENCE_ID "docs/evidencias/t8-1/rs-estimada-experimental.md"
#else
#define MQ135_RL_EVIDENCE_ID ""
#endif
#endif

namespace mq135_calibration_config {
// Valores NOMINALES ESTIMADOS, no mediciones del modulo ni del divisor.
constexpr double NOMINAL_INTERNAL_RL_OHMS = 10000.0;
constexpr double NOMINAL_DIVIDER_R1_OHMS = 10000.0;
constexpr double NOMINAL_DIVIDER_R2_OHMS = 10000.0;
inline mq135_calibration::LoadConfiguration loadConfiguration() {
  mq135_calibration::LoadConfiguration load{};
  load.effectiveLoadOhms = MQ135_EFFECTIVE_RL_OHMS;
  load.topologyVerified = MQ135_RL_TOPOLOGY_VERIFIED == 1;
  load.circuitRevision = MQ135_RL_CIRCUIT_REVISION;
  load.experimentalEstimate = MQ135_EXPERIMENTAL_RS == 1;
  constexpr char evidence[] = MQ135_RL_EVIDENCE_ID;
  static_assert(sizeof(evidence) <= sizeof(load.evidenceId),
                "Identificador de evidencia RL demasiado largo");
  std::memcpy(load.evidenceId, evidence, sizeof(evidence));
  return load;
}
}  // namespace mq135_calibration_config

#endif
