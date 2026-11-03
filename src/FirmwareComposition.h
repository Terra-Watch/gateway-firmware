#pragma once

#include "App.h"
#include "Gateway/GatewayService.h"
#include "PacketParser.h"
#include "PacketValidator.h"
#include "SerialLogger.h"
#include "Sx1278LoRaDriver.h"

namespace terrawatch {

class FirmwareComposition final {
public:
    FirmwareComposition();

    App& app();

private:
    logging::SerialLogger logger_;
    protocol::PacketParser parser_;
    protocol::PacketValidator validator_;
    lora::Sx1278LoRaDriver loraDriver_;
    gateway::GatewayService gatewayService_;
    App app_;
};

}  // namespace terrawatch
