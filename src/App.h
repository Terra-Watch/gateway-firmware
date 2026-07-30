#pragma once

#include "Gateway/GatewayService.h"
#include "ILoRaDriver.h"
#include "ILogger.h"

namespace jalri {

class App final {
public:
    App(lora::ILoRaDriver& loraDriver,
        gateway::GatewayService& gatewayService,
        logging::ILogger& logger);

    bool begin();
    void tick();

private:
    lora::ILoRaDriver& loraDriver_;
    gateway::GatewayService& gatewayService_;
    logging::ILogger& logger_;
};

}  // namespace jalri
