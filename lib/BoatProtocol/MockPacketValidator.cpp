#include "MockPacketValidator.h"

namespace jalri::protocol {

PacketValidationResult MockPacketValidator::validate(const ParsedPacketView& packet) const {
    if (packet.nodeId == 0) {
        return PacketValidationResult::invalidHeader;
    }
    return PacketValidationResult::success;
}

}  // namespace jalri::protocol

