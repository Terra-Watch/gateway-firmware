#include "ProtocolIntegrationTypes.h"

namespace jalri::protocol {

const char* toString(PacketParseResult result) {
    switch (result) {
        case PacketParseResult::success:
            return "Success";
        case PacketParseResult::parseFailed:
            return "ParseFailed";
    }

    return "ParseFailed";
}

const char* toString(PacketValidationResult result) {
    switch (result) {
        case PacketValidationResult::success:
            return "Success";
        case PacketValidationResult::invalidHeader:
            return "InvalidHeader";
        case PacketValidationResult::invalidVersion:
            return "InvalidVersion";
        case PacketValidationResult::invalidCrc:
            return "InvalidCRC";
        case PacketValidationResult::invalidLength:
            return "InvalidLength";
        case PacketValidationResult::invalidPacketType:
            return "InvalidPacketType";
        case PacketValidationResult::unsupportedProtocol:
            return "UnsupportedProtocol";
    }

    return "UnsupportedProtocol";
}

}  // namespace jalri::protocol

