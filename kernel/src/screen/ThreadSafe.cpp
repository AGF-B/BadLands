#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>

#include <screen/Log.hpp>

namespace {
    static Utils::Lock globalLogLock;
}

namespace Log {
    void putAtSafe(char c, uint32_t x, uint32_t y) {
        Utils::LockGuard _(globalLogLock);
        Log::putcAt(c, x, y);
    }

    void putcSafe(char c) {
        Utils::LockGuard _(globalLogLock);
        Log::putc(c);
    }

    void putsSafe(const char* s) {
        Utils::LockGuard _(globalLogLock);
        Log::puts(s);
    }

    void vprintfSafe(const char* format, va_list args) {
        Utils::LockGuard _(globalLogLock);
        Log::vprintf(format, args);
    }

    void printfSafe(const char* format, ...) {
        Utils::LockGuard _(globalLogLock);
        
        va_list args;
        va_start(args, format);
        Log::vprintf(format, args);
        va_end(args);
    }
}
