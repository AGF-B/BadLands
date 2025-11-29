#include <cstddef>
#include <cstdint>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/PIT.hpp>
#include <interrupts/Timer.hpp>

#include <mm/Heap.hpp>

#include <sched/Self.hpp>
#include <sched/TaskContext.hpp>

#include <screen/Log.hpp>

namespace {
    class _PITWrapper : public Timer {
    public:
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
            PIT::SetHandler(handler);
        }

        uint64_t GetCountMicros() const final {
            return PIT::GetCountMicros();
        }

        uint64_t GetCountMillis() const final {
            return PIT::GetCountMillis();
        }
    } static PITWrapper;

    static void IdleTask() {
        while (true) {
            __asm__ volatile("hlt");
        }
    }
}

UnattachedSelf::APICTimerWrapper::TimerProvider::TimerProvider(APICTimerWrapper* timer_wrapper) : timer_wrapper{timer_wrapper} {}

void UnattachedSelf::APICTimerWrapper::TimerProvider::HandleIRQ(void*,uint64_t) {
    timer_wrapper->InternalHandler();
}

void UnattachedSelf::APICTimerWrapper::InternalHandler() {
    SignalIRQ();

    if (handler != nullptr) {
        handler();
    }

    SendEOI();
}

void UnattachedSelf::APICTimerWrapper::Initialize() {
    static constexpr uint32_t TIMER_INITIAL_COUNT = 0xFFFFFFFF;
    static constexpr uint64_t CONFIG_GRANULARITY_MS = 19;

    int apic_timer_vector = Interrupts::ReserveInterrupt();
    if (apic_timer_vector <= 0) {
        Panic::PanicShutdown("COULD NOT RESERVE IRQ FOR APIC TIMER\n\r");
    }

    vector = static_cast<uint8_t>(apic_timer_vector);

    APIC::Timer::MaskTimerLVT();
    APIC::Timer::SetTimerDivideConfiguration(APIC::Timer::DivideConfiguration::BY_8);
    APIC::Timer::SetTimerLVT(vector, APIC::Timer::Mode::PERIODIC);

    PIT::Enable();

    const uint64_t target = PIT::GetCountMillis() + CONFIG_GRANULARITY_MS;
    APIC::Timer::SetTimerInitialCount(TIMER_INITIAL_COUNT);

    while (PIT::GetCountMillis() < target) {
        __asm__ volatile("pause");
    }

    const uint64_t end_count = APIC::Timer::GetTimerCurrentCount();
    const uint64_t ticks = (0xFFFFFFFF - end_count) / CONFIG_GRANULARITY_MS;

    PIT::Disable();

    APIC::Timer::SetTimerInitialCount(static_cast<uint32_t>(ticks));

    Interrupts::RegisterIRQ(vector, &provider);
    APIC::Timer::UnmaskTimerLVT();

    Log::printfSafe("[CPU %u] Configured APIC timer for 1ms intervals\n\r", APIC::GetLAPICID());
}

bool UnattachedSelf::APICTimerWrapper::IsEnabled() const {
    return enabled;
}

void UnattachedSelf::APICTimerWrapper::Enable() {
    APIC::Timer::UnmaskTimerLVT();
    enabled = true;
}

void UnattachedSelf::APICTimerWrapper::Disable() {
    APIC::Timer::MaskTimerLVT();
    enabled = false;
}

void UnattachedSelf::APICTimerWrapper::ReattachIRQ(void (*handler)()) {
    if (vector != 0) {
        Interrupts::ForceIRQHandler(vector, reinterpret_cast<void*>(handler));
    }
}

void UnattachedSelf::APICTimerWrapper::ReleaseIRQ() {
    if (vector != 0) {
        Interrupts::ReleaseIRQ(vector);
    }
}

void UnattachedSelf::APICTimerWrapper::SignalIRQ() {
    millis_counter += MILLIS_INTERVAL;
}

void UnattachedSelf::APICTimerWrapper::SendEOI() const {
    APIC::SendEOI();
}

void UnattachedSelf::APICTimerWrapper::SetHandler(void (*handler)()) {
    this->handler = handler;
}

uint64_t UnattachedSelf::APICTimerWrapper::GetCountMicros() const {
    return millis_counter * 1000;
}

uint64_t UnattachedSelf::APICTimerWrapper::GetCountMillis() const {
    return millis_counter;
}

UnattachedSelf::UnattachedSelf(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable)
    : enabled(enabled), online_capable(online_capable), apic_id(apic_id), apic_uid(apic_uid) {
    
    auto idle_context = Scheduling::KernelTaskContext::Create(reinterpret_cast<void*>(&IdleTask));
    if (!idle_context.HasValue()) {
        Panic::PanicShutdown("COULD NOT CREATE IDLE TASK\n\r");
    }

    // create non-blockable idle task
    task_manager.AddTask(idle_context.GetValue(), false);
}

UnattachedSelf* UnattachedSelf::AllocateProcessors(size_t count) {
    if (processors == nullptr && count > 0) {
        processors = static_cast<UnattachedSelf*>(Heap::Allocate(count * sizeof(UnattachedSelf)));
        processor_count = count;
    }

    return processors;
}

UnattachedSelf& UnattachedSelf::AccessRemote(uint8_t id) {
    if (id >= processor_count) {
        Panic::Panic("ILLEGAL ACCESS TO INVALID REMOTE PROCESSOR\n\r");
    }

    return processors[id];
}

UnattachedSelf& UnattachedSelf::Attach() {
    uint8_t apic_id = APIC::GetLAPICID();

    for (size_t i = 0; i < processor_count; i++) {
        if (processors[i].apic_id == apic_id) {
            return processors[i];
        }
    }

    Panic::Panic("COULD NOT FIND OWN PROCESSOR\n\r");
}

bool UnattachedSelf::IsEnabled() const {
    return enabled;
}

bool UnattachedSelf::IsOnlineCapable() const {
    return online_capable;
}

uint8_t UnattachedSelf::GetID() const {
    return apic_id;
}

void UnattachedSelf::Reset() {
    if (enabled) {
        // TODO: release all memory
    }

    // reset APIC
    
    // wait for the processor to be ready

    // send reset IPI

    Panic::Panic("COULD NOT RESET PROCESSOR\n\r");
}

void UnattachedSelf::ForceHaltRemote() {
    Panic::Panic("REMOTE FORCE HALT NOT IMPLEMENTED\n\r");
}

[[noreturn]] void UnattachedSelf::ForceHalt() {
    __asm__ volatile("cli");
    while (true) {
        __asm__ volatile("hlt");
    }
}

void UnattachedSelf::SpinWaitMillis(uint64_t ms) const {
    const uint64_t target = local_timer.GetCountMillis() + ms;

    while (local_timer.GetCountMillis() < target) {
        __asm__ volatile("pause");
    }
}

bool UnattachedSelf::SpinWaitMillsFor(uint64_t ms, bool (*predicate)(void*), void* args) const {
    const uint64_t target = local_timer.GetCountMillis() + ms;

    while (local_timer.GetCountMillis() < target) {
        if (predicate(args)) {
            return true;
        }
        __asm__ volatile("pause");
    }

    return false;
}

Timer& UnattachedSelf::GetPIT() {
    return PITWrapper;
}

Timer& UnattachedSelf::GetTimer() {
    return local_timer;
}

Scheduling::TaskManager& UnattachedSelf::GetTaskManager() {
    return task_manager;
}

UnattachedSelf& Self() {
    return UnattachedSelf::Attach();
}
