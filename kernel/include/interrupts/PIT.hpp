#pragma once

#include <interrupts/InterruptProvider.hpp>

namespace PIT {
    void Initialize();
    bool IsEnabled();
    void Enable();
    void Disable();
    void ReattachIRQ(void (*handler)());
    void ReleaseIRQ();
    void SignalIRQ();
    void SendEOI();
    void SetHandler(void (*handler)());
    void HandleInterrupt();
    uint64_t GetCountMicros();
    uint64_t GetCountMillis();
}
