#pragma once

#include <interrupts/Timer.hpp>

#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

class Self {
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

    static inline Self* processors;
    static inline size_t processor_count;

    bool enabled{false};
    bool online_capable{false};

    uint8_t apic_id{0xFF};
    uint8_t apic_uid{0xFF};

    APICTimerWrapper local_timer;
    Scheduling::TaskManager task_manager;

public:
    Self(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable);

    static Self& Get();

    bool IsEnabled() const;
    bool IsOnlineCapable() const;
    void Reset();
    void ForceHaltRemote();
    [[noreturn]] static void ForceHalt();

    Timer& GetPIT();
    Timer& GetTimer();
    
    void AddTask(Scheduling::TaskContext& context);
    void RemoveTask(uint64_t task_id);
    void BlockTask(uint64_t task_id);
    void UnblockTask(uint64_t task_id);
    Scheduling::TaskContext* TaskSwitch(void* stack_context);
};
