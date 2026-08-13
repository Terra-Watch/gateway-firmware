#pragma once

#include "IPacketValidator.h"

namespace jalri::protocol {

class PacketValidator final : public IPacketValidator {
public:
    PacketValidationResult validate(const ParsedPacketView& packet) const override;
};

}  // namespace jalri::protocol
