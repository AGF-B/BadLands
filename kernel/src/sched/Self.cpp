#include <cstdint>

#include <interrupts/APIC.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/PIT.hpp>
#include <interrupts/Timer.hpp>

#include <sched/Self.hpp>

namespace {
    class _PITWrapper : public Timer {
        void Initialize() final {
            PIT::Initialize();
        }

        bool IsEnabled() const final {
            return PIT::IsEnabled();
        }

        void Enable() final {
            PIT::Enable();
        }

        void Disable() final {
            PIT::Disable();
        }

        void ReattachIRQ(void (*handler)()) final {
            PIT::ReattachIRQ(handler);
        }

        void ReleaseIRQ() final {
            PIT::ReleaseIRQ();
        }

        void SignalIRQ() final {
            PIT::SignalIRQ();
        }

        void SendEOI() const final {
            PIT::SendEOI();
        }

        void SetHandler(void (*handler)()) final {
            PIT::ReattachIRQ(handler);
        }

        uint64_t GetCountMicros() const final {
            return PIT::GetCountMicros();
        }

        uint64_t GetcountMillis() const final {
            return PIT::GetCountMillis();
        }
    } PITWrapper;
}

Self::Self(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable)
    : apic_id(apic_id), apic_uid(apic_uid), enabled(enabled), online_capable(online_capable) {
}

Self& Self::Get() {
    uint8_t apic_id = APIC::GetLAPICID();

    for (size_t i = 0; i < processor_count; i++) {
        if (processors[i].apic_id == apic_id) {
            return processors[i];
        }
    }

    Panic::Panic("COULD NOT FIND OWN PROCESSOR\n\r");
}

bool Self::IsEnabled() const {
    return enabled;
}

bool Self::IsOnlineCapable() const {
    return online_capable;
}

void Self::Reset() {
    if (enabled) {
        // TODO: release all memory
    }

    // reset APIC
    
    // wait for the processor to be ready

    // send reset IPI

    Panic::Panic("COULD NOT RESET PROCESSOR\n\r");
}

void Self::ForceHalt() {
    __asm__ volatile("cli");
    while (true) {
        __asm__ volatile("hlt");
    }
}

Timer& Self::GetPIT() {
    return PITWrapper;
}

Timer& Self::GetTimer() {
    // TODO: make APIC return its timer
    Panic::PanicShutdown("APIC TIMER NOT IMPLEMENTED\n\r");
}

void Self::AddTask(Scheduling::TaskContext& context) {
    task_manager.AddTask(context);
}

void Self::RemoveTask(uint64_t task_id) {
    task_manager.RemoveTask(task_id);
}

void Self::BlockTask(uint64_t task_id) {
    task_manager.BlockTask(task_id);
}

void Self::UnblockTask(uint64_t task_id) {
    task_manager.UnblockTask(task_id);
}
