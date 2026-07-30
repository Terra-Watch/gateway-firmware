#pragma once

#include <array>
#include <cstdint>

#include "ILoRaDriver.h"

namespace jalri::lora {

class Sx1278LoRaDriver final : public ILoRaDriver {
public:
    bool begin() override;
    bool receive(RawRadioPacket& packet) override;

private:
    static constexpr long frequencyHz = 433000000L;
    static constexpr int spreadingFactor = 7;
    static constexpr long signalBandwidthHz = 125000L;
    static constexpr int codingRateDenominator = 5;
    static constexpr int syncWord = 0x12;

    std::array<std::uint8_t, 255> rxBuffer_{};
};

}  // namespace jalri::lora
