#pragma once

#include "IPacketParser.h"

namespace terrawatch::protocol {

class MockPacketParser final : public IPacketParser {
public:
    PacketParseResult parse(const std::uint8_t* bytes,
                            std::size_t length,
                            ParsedPacketView& packet) const override;
};

}  // namespace terrawatch::protocol

