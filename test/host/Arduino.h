#pragma once

#include <cstdarg>

struct TestSerial {
    template <typename... Args>
    void printf(const char*, Args...) {}
    void println(const char*) {}
    void flush() {}
    explicit operator bool() const { return true; }
};

extern TestSerial Serial;
