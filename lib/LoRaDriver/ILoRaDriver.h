#pragma once

#include "LoRaTypes.h"

namespace jalri::lora {

class ILoRaDriver {
public:
    virtual ~ILoRaDriver() = default;

    virtual bool begin() = 0;
    virtual bool receive(RawRadioPacket& packet) = 0;
    virtual bool send(const std::uint8_t* data, std::size_t length) = 0;
};

}  // namespace jalri::lora
