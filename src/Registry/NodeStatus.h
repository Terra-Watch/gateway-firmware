#pragma once

namespace jalri::registry {

enum class NodeStatus {
    online,
    offline
};

inline const char* toString(NodeStatus status) {
    switch (status) {
        case NodeStatus::online:
            return "ONLINE";
        case NodeStatus::offline:
            return "OFFLINE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace jalri::registry
