#pragma once

#include <cstddef>
#include <cstdint>

#include <interrupts/Timer.hpp>

#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

class UnattachedSelf {
private:
    class APICTimerWrapper : public Timer {
    private:
        class TimerProvider : public Interrupts::InterruptProvider {
        private:
            APICTimerWrapper* const timer_wrapper;

        public:
            TimerProvider(APICTimerWrapper* timer_wrapper);
            void HandleIRQ(void*,uint64_t);
        };

        static constexpr uint8_t MILLIS_INTERVAL = 1;

        uint8_t vector = 0;
        bool enabled{false};
        void (*handler)() = nullptr;
        uint64_t millis_counter = 0;
        TimerProvider provider{this};

        void InternalHandler();

    public:
        void Initialize() final;
        bool IsEnabled() const final;
        void Enable() final;
        void Disable() final;
        void ReattachIRQ(void (*handler)()) final;
        void ReleaseIRQ() final;
        void SignalIRQ() final;
        void SendEOI() const final;
        void SetHandler(void (*handler)()) final;
        uint64_t GetCountMicros() const final;
        uint64_t GetCountMillis() const final;
    };

    static inline UnattachedSelf* processors = nullptr;
    static inline size_t processor_count = 0;
    static inline size_t allocated_processors = 0;

    bool enabled{false};
    bool online_capable{false};

    uint8_t apic_id{0xFF};
    uint8_t apic_uid{0xFF};

    APICTimerWrapper local_timer;
    Scheduling::TaskManager task_manager;

public:
    UnattachedSelf(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable);

    static UnattachedSelf* AllocateProcessors(size_t count);
    static UnattachedSelf& AllocateRemote();
    static UnattachedSelf& AccessRemote(uint8_t id);
    static UnattachedSelf& Attach();

    bool IsEnabled() const;
    bool IsOnlineCapable() const;
    uint8_t GetID() const;
    void Reset();
    void ForceHaltRemote();
    [[noreturn]] static void ForceHalt();

    void SpinWaitMillis(uint64_t ms) const;
    bool SpinWaitMillsFor(uint64_t ms, bool (*predicate)(void*), void* args) const;

    static Timer& GetPIT();
    Timer& GetTimer();

    Scheduling::TaskManager& GetTaskManager();
};

UnattachedSelf& Self();
