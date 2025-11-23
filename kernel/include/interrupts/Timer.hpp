#pragma once

#include <cstdint>

#include <interrupts/InterruptProvider.hpp>

class Timer {
    virtual void Initialize() = 0;
    virtual bool IsEnabled() const = 0;
    virtual void Enable() = 0;
    virtual void Disable() = 0;
    virtual void ReattachIRQ(void (*handler)()) = 0;
    virtual void ReleaseIRQ() = 0;
    virtual void SignalIRQ() = 0;
    virtual void SendEOI() const = 0;
    virtual void SetHandler(void (*handler)()) = 0;
    virtual uint64_t GetCountMicros() const = 0;
    virtual uint64_t GetcountMillis() const = 0;
};
