#pragma once

#include <cstddef>
#include <cstdint>

namespace terrawatch::lora {

struct RawRadioPacket {
    const std::uint8_t* bytes;
    std::size_t length;
    int rssiDbm;
    float snrDb;
    std::uint32_t receivedAtMs;
};

using LoRaPacket = RawRadioPacket;

}  // namespace terrawatch::lora
