#include "MockPacketValidator.h"

namespace terrawatch::protocol {

PacketValidationResult MockPacketValidator::validate(const ParsedPacketView& packet) const {
    if (packet.nodeId == 0) {
        return PacketValidationResult::invalidHeader;
    }
    return PacketValidationResult::success;
}

}  // namespace terrawatch::protocol

