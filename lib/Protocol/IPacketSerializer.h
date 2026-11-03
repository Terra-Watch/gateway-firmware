#pragma once

#include <cstddef>
#include <cstdint>

#include "ProtocolIntegrationTypes.h"

namespace terrawatch::protocol {

class IPacketSerializer {
public:
    virtual ~IPacketSerializer() = default;

    virtual bool serialize(const ParsedPacketView& packet,
                           std::uint8_t* output,
                           std::size_t outputCapacity,
                           std::size_t& bytesWritten) const = 0;
};

}  // namespace terrawatch::protocol

