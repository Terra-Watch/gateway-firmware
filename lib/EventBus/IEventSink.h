#pragma once

namespace jalri::events {

template <typename Event>
class IEventSink {
public:
    virtual ~IEventSink() = default;

    virtual void publish(const Event& event) = 0;
};

}  // namespace jalri::events

