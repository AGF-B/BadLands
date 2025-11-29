#pragma once

#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/Response.hpp>

#include <devices/USB/xHCI/TRB.hpp>
#include <interrupts/InterruptProvider.hpp>
#include <pci/Interface.hpp>

namespace Devices {
    namespace USB {
        namespace xHCI {
            class Controller : public Interrupts::InterruptProvider {
                private:
                struct CapabilityRegisters {
                    uint8_t     CAPLENGTH;
                    uint8_t     _pad;
                    uint16_t    HCIVERSION;
                    uint32_t    HCSPARAMS1;
                    uint32_t    HCSPARAMS2;
                    uint32_t    HCSPARAMS3;
                    uint32_t    HCCPARAMS1;
                    uint32_t    DBOFF;
                    uint32_t    RTSOFF;
                    uint32_t    HCCPARAMS2;
                };

                struct OperationalRegisters {
                    uint32_t    USBCMD;
                    uint32_t    USBSTS;
                    uint32_t    PAGESIZE;
                    uint32_t    _pad0;
                    uint32_t    _pad1;
                    uint32_t    DNCTRL;
                    uint32_t    CRCR_LO;
                    uint32_t    CRCR_HI;
                    uint64_t    _pad2;
                    uint64_t    _pad3;
                    uint32_t    DCBAAP_LO;
                    uint32_t    DCBAAP_HI;
                    uint32_t    CONFIG;
                };

                class PortSpeed {
                public:
                    enum {
                        InvalidSpeed,
                        LowSpeed,
                        FullSpeed,
                        HighSpeed,
                        SuperSpeedGen1x1,
                        SuperSpeedPlusGen1x2,
                        SuperSpeedPlusGen2x1,
                        SuperSpeedPlusGen2x2
                    };
                    
                    decltype(InvalidSpeed) value;

                    static PortSpeed FromSpeedID(uint8_t id);

                    bool operator==(const decltype(InvalidSpeed)& speed) const;
                    bool operator!=(const decltype(InvalidSpeed)& speed) const;
                };

                struct OperationalPort {
                    static constexpr uint32_t PORTSC_CCS    = 0x00000001;
                    static constexpr uint32_t PORTSC_PED    = 0x00000002;
                    static constexpr uint32_t PORTSC_PR     = 0x00000010;
                    static constexpr uint32_t PORTSC_PP     = 0x00000200;
                    static constexpr uint32_t PORTSC_CSC    = 0x00020000;
                    static constexpr uint32_t PORTSC_PRC    = 0x00200000;

                    uint32_t PORTSC;
                    uint32_t PORTPMSC;
                    uint32_t PORTLI;
                    uint32_t PORTHLPMC;

                    uint8_t GetSpeedID() const;
                };

                struct RuntimeRegisters {
                    struct IRS {
                        uint32_t IMAN;
                        uint32_t IMOD;
                        uint32_t ERSTSZ;
                        uint32_t _pad;
                        uint64_t ERSTBA;
                        uint64_t ERDP;
                    };

                    uint32_t    MFINDEX;
                    alignas(0x20) IRS InterrupterRegisterSets[1024];
                };

                struct DoorbellRegisters {
                    uint32_t r[256];
                };

                struct ExtendedCapability {
                protected:
                    uint32_t GetDWord(uint64_t i) const;
                    uint8_t GetByte(uint64_t i) const;
                
                public:
                    uint8_t GetID() const;
                    uint8_t GetNextPointer() const;
                };

                struct SupportedProtocolCapability : public ExtendedCapability {
                    uint8_t GetMinorRevision() const;
                    uint8_t GetMajorRevision() const;
                    uint32_t GetNameString() const;
                    uint8_t GetCompatiblePortOffset() const;
                    uint8_t GetCompatiblePortCount() const;
                    uint16_t GetProtocolDefined() const;
                    uint8_t GetSpeedIDCount() const;
                    uint8_t GetProtocolSlotType() const;
                };

                struct DCBAA {
                    void* ptr;
                };

                struct ERSTEntry {
                    uint32_t data[4];

                    static ERSTEntry Create(void* ptr, uint16_t size);
                };

                struct Port {
                    uint8_t major;
                    uint8_t slot;
                    uint8_t slot_type;
                    bool dirty;
                };

                const PCI::IType0 interface;

                void* MMIO_base = nullptr;

                CapabilityRegisters* capability = nullptr;
                OperationalRegisters* opregs = nullptr;
                RuntimeRegisters* primary_rtregs = nullptr;
                DoorbellRegisters* doorbell_regs = nullptr;

                uint8_t max_slots_enabled = 0;

                DCBAA* dcbaa = nullptr;

                Utils::Lock command_lock;
                TRB* command_ring = nullptr;
                CommandCompletionEventTRB command_completion;
                size_t command_index = 0;
                bool command_cycle = true;
                size_t command_capacity = 0;

                EventTRB* event_ring = nullptr;
                ERSTEntry* event_ring_segment_table = nullptr;
                size_t event_index = 0;
                bool event_cycle = true;
                size_t event_capacity = 0;

                void** scratchpad_buffer_array = nullptr;
                uint16_t max_scratchpad_buffers = 0;

                Port* ports = nullptr;
                mutable bool port_update = false;
                uint64_t port_updater_task_id = 0;

                int interrupt_vector = -1;

                Controller(const PCI::Interface& interface);

                void HandleIRQ(void* stack, uint64_t error_code) final;

                void ReleaseResources();

                bool ConfigureMMIO();
                void ReleaseMMIO();

                void ConfigureMaxSlotsEnabled();

                void ConfigurePrimaryInterrupter() const;
                RuntimeRegisters::IRS* GetPrimaryInterrupter() const;
                void EnablePrimaryInterrupter() const;
                void DisablePrimaryInterrupter() const;

                size_t GetDCBAAPPages() const;
                bool ConfigureDCBAAP();
                void ReleaseDCBAAP();
                void WriteDCBAAEntry(uint8_t slot, void* ptr) const;

                static constexpr size_t GetCommandRingPages();
                bool ConfigureCommandRing();
                void ReleaseCommandRing();
                void UpdateCommandPointer();
                void EnqueueCommand(const CommandTRB& trb) const;
                void SignalCommand() const;

                static constexpr size_t GetEventRingPages();
                static constexpr uint8_t GetEventRingSegments();
                static constexpr uint64_t GetEventRingSegmentTableOffset();
                bool ConfigureEventRing();
                void ReleaseEventRing();
                void UpdateEventPointer();
                EventTRB* GetCurrentEvent() const;
                bool GetEventCycle() const;

                size_t GetScratchpadPages() const;
                bool ConfigureScratchpad();
                void ReleaseScratchpad();

                uint8_t GetMaxPorts() const;
                OperationalPort* AccessOperationalPort(size_t portid) const;
                bool ConfigurePortVersions();
                static void UpdatePortsTrampoline(Controller* controller);
                void UpdatePorts();
                void ReleasePorts();

                int16_t EnableSlot(uint8_t slot_type);
                void DisableSlot(uint8_t id) const;

                ExtendedCapability* FindExtendedCapability(uint8_t id) const;
                ExtendedCapability* FindNextExtendedCapability(uint8_t id, ExtendedCapability* ptr) const;
                SupportedProtocolCapability* FindSupportedProtocolCapability() const;
                
            public:
                static Controller* Initialize(
                    uint8_t bus,
                    uint8_t device,
                    uint8_t function,
                    void* configuration_space
                );
                static void Release(Controller* c);

                bool ResetHost() const;
                void EnableHost() const;
                void DisableHost() const;
                void EnableHostInterrupts() const;
                void DisableHostInterrupts() const;

                bool GetCommandCycle() const;
                Optional<CommandCompletionEventTRB> SendCommand(const CommandTRB& trb);

                void Destroy();
            };
        }
    }
}
