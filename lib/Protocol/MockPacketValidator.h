#pragma once

#include "IPacketValidator.h"

namespace terrawatch::protocol {

class MockPacketValidator final : public IPacketValidator {
public:
    PacketValidationResult validate(const ParsedPacketView& packet) const override;
};

}  // namespace terrawatch::protocol

