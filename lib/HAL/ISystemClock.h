#pragma once

#include <cstdint>

namespace jalri::hal {

class ISystemClock {
public:
    virtual ~ISystemClock() = default;

    virtual std::uint32_t millis() const = 0;
};

}  // namespace jalri::hal

