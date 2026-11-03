#pragma once

#include <cstdint>

namespace terrawatch::hal {

class ISystemClock {
public:
    virtual ~ISystemClock() = default;

    virtual std::uint32_t millis() const = 0;
};

}  // namespace terrawatch::hal

