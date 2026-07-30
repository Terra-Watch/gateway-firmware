#pragma once

#include "App.h"
#include "Gateway/GatewayService.h"
#include "MockPacketParser.h"
#include "MockPacketValidator.h"
#include "SerialLogger.h"
#include "Sx1278LoRaDriver.h"

namespace jalri {

class FirmwareComposition final {
public:
    FirmwareComposition();

    App& app();

private:
    logging::SerialLogger logger_;
    protocol::MockPacketParser parser_;
    protocol::MockPacketValidator validator_;
    lora::Sx1278LoRaDriver loraDriver_;
    gateway::GatewayService gatewayService_;
    App app_;
};

}  // namespace jalri
