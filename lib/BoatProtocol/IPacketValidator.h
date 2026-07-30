#pragma once

#include "ProtocolIntegrationTypes.h"

namespace jalri::protocol {

class IPacketValidator {
public:
    virtual ~IPacketValidator() = default;

    virtual PacketValidationResult validate(const ParsedPacketView& packet) const = 0;
};

}  // namespace jalri::protocol

