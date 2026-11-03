#pragma once

#include "IPacketValidator.h"

namespace terrawatch::protocol {

class PacketValidator final : public IPacketValidator {
public:
    PacketValidationResult validate(const ParsedPacketView& packet) const override;
};

}  // namespace terrawatch::protocol
