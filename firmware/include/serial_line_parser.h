#ifndef MONITOREO_IOT_SERIAL_LINE_PARSER_H
#define MONITOREO_IOT_SERIAL_LINE_PARSER_H

#include <cstddef>
#include <cstring>

namespace serial_line_parser {
// Parser incremental compartido por todos los comandos; sin Arduino ni esperas.
class Parser {
 public:
  template <typename Line, typename Rejected>
  void feed(char character, Line line, Rejected rejected) {
    if (character == '\r' || character == '\n') {
      if (invalid_) rejected();
      else if (length_ > 0) { command_[length_] = '\0'; line(command_); }
      length_ = 0;
      invalid_ = false;
    } else if (!invalid_) {
      if (character == '\0' || length_ + 1 >= sizeof(command_)) invalid_ = true;
      else command_[length_++] = character;
    }
  }
 private:
  char command_[64]{};
  std::size_t length_ = 0;
  bool invalid_ = false;
};

enum class CalibrationCommand { None, Start, State, Stop };
inline CalibrationCommand calibrationCommand(const char* command) {
  if (std::strcmp(command, "mq135 calibracion iniciar") == 0)
    return CalibrationCommand::Start;
  if (std::strcmp(command, "mq135 calibracion estado") == 0)
    return CalibrationCommand::State;
  if (std::strcmp(command, "mq135 calibracion detener") == 0)
    return CalibrationCommand::Stop;
  return CalibrationCommand::None;
}
}  // namespace serial_line_parser

#endif
