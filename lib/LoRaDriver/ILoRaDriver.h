#pragma once

#include "LoRaTypes.h"

namespace jalri::lora {

class ILoRaDriver {
public:
    virtual ~ILoRaDriver() = default;

    virtual bool begin() = 0;
    virtual bool receive(LoRaPacket& packet) = 0;
};

}  // namespace jalri::lora

