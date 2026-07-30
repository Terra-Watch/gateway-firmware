#pragma once

#include <cstddef>
#include <cstdint>

namespace jalri::hal {

class ISpiBus {
public:
    virtual ~ISpiBus() = default;

    virtual bool transfer(const std::uint8_t* tx,
                          std::uint8_t* rx,
                          std::size_t length) = 0;
};

}  // namespace jalri::hal

