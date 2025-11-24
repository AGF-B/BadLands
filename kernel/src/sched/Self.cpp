#include <cstdint>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/PIT.hpp>
#include <interrupts/Timer.hpp>

#include <sched/Self.hpp>

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
            PIT::ReattachIRQ(handler);
        }

        uint64_t GetCountMicros() const final {
            return PIT::GetCountMicros();
        }

        uint64_t GetCountMillis() const final {
            return PIT::GetCountMillis();
        }
    } PITWrapper;
}

Self::APICTimerWrapper::TimerProvider::TimerProvider(APICTimerWrapper* timer_wrapper) : timer_wrapper{timer_wrapper} {}

void Self::APICTimerWrapper::TimerProvider::HandleIRQ(void*,uint64_t) {
    timer_wrapper->InternalHandler();
}

void Self::APICTimerWrapper::InternalHandler() {
    SignalIRQ();

    if (handler != nullptr) {
        handler();
    }

    SendEOI();
}

void Self::APICTimerWrapper::Initialize() {
    static constexpr uint32_t TIMER_INITIAL_COUNT = 0xFFFFFFFF;
    static constexpr uint64_t CONFIG_GRANULARITY_MS = 19;

    int apic_timer_vector = Interrupts::ReserveInterrupt();
    if (apic_timer_vector < 0) {
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

bool Self::APICTimerWrapper::IsEnabled() const {
    return enabled;
}

void Self::APICTimerWrapper::Enable() {
    APIC::Timer::UnmaskTimerLVT();
    enabled = true;
}

void Self::APICTimerWrapper::Disable() {
    APIC::Timer::MaskTimerLVT();
    enabled = false;
}

void Self::APICTimerWrapper::ReattachIRQ(void (*handler)()) {
    if (vector != 0) {
        Interrupts::ForceIRQHandler(vector, reinterpret_cast<void*>(handler));
    }
}

void Self::APICTimerWrapper::ReleaseIRQ() {
    if (vector != 0) {
        Interrupts::ReleaseIRQ(vector);
    }
}

void Self::APICTimerWrapper::SignalIRQ() {
    millis_counter += MILLIS_INTERVAL;
}

void Self::APICTimerWrapper::SendEOI() const {
    APIC::SendEOI();
}

void Self::APICTimerWrapper::SetHandler(void (*handler)()) {
    this->handler = handler;
}

uint64_t Self::APICTimerWrapper::GetCountMicros() const {
    return millis_counter * 1000;
}

uint64_t Self::APICTimerWrapper::GetCountMillis() const {
    return millis_counter;
}

Self::Self(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable)
    : enabled(enabled), online_capable(online_capable), apic_id(apic_id), apic_uid(apic_uid) {
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

void Self::ForceHaltRemote() {
    Panic::Panic("REMOTE FORCE HALT NOT IMPLEMENTED\n\r");
}

[[noreturn]] void Self::ForceHalt() {
    __asm__ volatile("cli");
    while (true) {
        __asm__ volatile("hlt");
    }
}

Timer& Self::GetPIT() {
    return PITWrapper;
}

Timer& Self::GetTimer() {
    return local_timer;
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
