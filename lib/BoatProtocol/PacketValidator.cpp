#include "PacketValidator.h"

namespace jalri::protocol {

PacketValidationResult PacketValidator::validate(const ParsedPacketView& packet) const {
    if (packet.protocolVersion != 4U || packet.nodeId == 0U || packet.previousHopId == 0U) {
        return packet.protocolVersion != 4U ? PacketValidationResult::invalidVersion
                                            : PacketValidationResult::invalidHeader;
    }
    if (packet.payloadLength > 48U) {
        return PacketValidationResult::invalidLength;
    }

    switch (packet.packetType) {
    case 1U:
        return PacketValidationResult::success;
    case 2U:
        return packet.payloadLength == 13U ? PacketValidationResult::success
                                           : PacketValidationResult::invalidLength;
    case 3U:
        return packet.payloadLength == 2U ? PacketValidationResult::success
                                          : PacketValidationResult::invalidLength;
    case 4U:
        return packet.payloadLength == 9U ? PacketValidationResult::success
                                          : PacketValidationResult::invalidLength;
    default:
        return PacketValidationResult::invalidPacketType;
    }
}

}  // namespace jalri::protocol
