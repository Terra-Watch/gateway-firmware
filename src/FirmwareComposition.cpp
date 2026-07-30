#include "FirmwareComposition.h"

namespace jalri {

FirmwareComposition::FirmwareComposition()
    : logger_(),
      parser_(),
      validator_(),
      loraDriver_(),
      gatewayService_(parser_, validator_, logger_),
      app_(loraDriver_, gatewayService_, logger_) {}

App& FirmwareComposition::app() {
    return app_;
}

}  // namespace jalri
