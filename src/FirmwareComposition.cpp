#include "FirmwareComposition.h"
#include "BuildConfig.h"

namespace jalri {

FirmwareComposition::FirmwareComposition()
    : logger_(),
      parser_(),
      validator_(),
      loraDriver_(),
      backendClient_({BackendBuildConfig::wifiSsid,
                      BackendBuildConfig::wifiPassword,
                      BackendBuildConfig::baseUrl,
                      BackendBuildConfig::gatewayId}),
      gatewayService_(parser_, validator_, logger_, backendClient_),
      app_(loraDriver_, gatewayService_, logger_) {}

App& FirmwareComposition::app() {
    return app_;
}

}  // namespace jalri
