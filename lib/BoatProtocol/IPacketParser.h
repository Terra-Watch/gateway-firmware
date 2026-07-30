#pragma once

#include <cstddef>
#include <cstdint>

#include "ProtocolIntegrationTypes.h"

namespace jalri::protocol {

class IPacketParser {
public:
    virtual ~IPacketParser() = default;

    virtual PacketParseResult parse(const std::uint8_t* bytes,
                                    std::size_t length,
                                    ParsedPacketView& packet) const = 0;
};

}  // namespace jalri::protocol

