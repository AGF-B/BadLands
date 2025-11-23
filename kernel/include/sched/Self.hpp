#pragma once

#include <interrupts/Timer.hpp>

#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

class Self {
private:
    static Self* processors;
    static size_t processor_count;

    bool enabled = false;
    bool online_capable = false;

    uint8_t apic_id;
    uint8_t apic_uid;

    Scheduling::TaskManager task_manager;

public:
    Self(uint8_t apic_id, uint8_t apic_uid, bool enabled, bool online_capable);

    static Self& Get();

    bool IsEnabled() const;
    bool IsOnlineCapable() const;
    void Reset();
    void ForceHalt();

    Timer& GetPIT();
    Timer& GetTimer();
    
    void AddTask(Scheduling::TaskContext& context);
    void RemoveTask(uint64_t task_id);
    void BlockTask(uint64_t task_id);
    void UnblockTask(uint64_t task_id);

    
};
