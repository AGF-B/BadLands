#pragma once

#include <cstdint>

namespace Devices {
    namespace USB {
        namespace xHCI {
            struct TRB {
                enum class CompletionCode : uint8_t {
                    Invalid = 0,
                    Success,
                    DataBufferError,
                    BabbleDetectedError,
                    USBTransactionError,
                    TRBError,
                    StallError,
                    ResourceError,
                    BandwidthError,
                    NoSlotsAvailableError,
                    InvalidStreamTypeError,
                    SlotNotEnabledError,
                    EndpointNotEnabledError,
                    ShortPacket,
                    RingUnderrun,
                    RingOverrun,
                    VFEventRingFullError,
                    ParameterError,
                    BandwidthOverrunError,
                    ContextStateError,
                    NoPingResponseError,
                    EventRingFullError,
                    IncompatibleDeviceError,
                    MissedServiceError,
                    CommandRingStoppedError,
                    CommandAbortedError,
                    Stopped,
                    StoppedLengthInvalid,
                    StoppedShortPacket,
                    MaxExitLatencyTooLargeError,
                    IsochronousBufferOverrunError = 31,
                    EventLostError = 32,
                    UndefinedError = 33,
                    InvalidStreamIDError = 34,
                    SecondaryBandwidthError = 35,
                    SplitTransactionError = 36
                };

                uint32_t data[4];

                uint8_t GetSlotType() const;
                bool GetCycle() const;
                void SetCycle(bool cycle);
                void SetTRBType(uint8_t type);
            };

            struct EventTRB : public TRB {
                enum class Type : uint8_t {
                    Unknown,
                    TransferEvent = 32,
                    CommandCompletionEvent,
                    PortStatusChangeEvent,
                    BandwidthRequestEvent,
                    DoorbellEvent,
                    HostControllerEvent,
                    DeviceNotificationEvent,
                    MFINDEXWrapEvent
                };

                CompletionCode GetCompletionCode() const;
                Type GetType() const;

            protected:
                uint64_t GetEventData() const;
                TRB* GetPointer() const;
                uint32_t GetEventParameter() const;
                uint8_t GetVFID() const;
                uint8_t GetSlotID() const;
            };

            struct TransferEventTRB : public EventTRB {
                using EventTRB::GetPointer;
                using EventTRB::GetEventData;
                using EventTRB::GetEventParameter;
                using EventTRB::GetSlotID;

                bool GetEventDataPresent() const;
                uint8_t GetEndpointID() const;
            };

            struct CommandCompletionEventTRB : public EventTRB {
                using EventTRB::GetPointer;
                using EventTRB::GetEventParameter;
                using EventTRB::GetVFID;
                using EventTRB::GetSlotID;
            };

            struct PortStatusChangeEventTRB : public EventTRB {
                uint8_t GetPortID() const;
            };

            struct CommandTRB : public TRB {
            protected:
                using TRB::SetCycle;
                using TRB::SetTRBType;
                void SetSlotType(uint8_t type);
                void SetSlotID(uint8_t id);
            };

            struct NoOpTRB : public CommandTRB {
                static NoOpTRB Create(bool cycle);
            };

            struct EnableSlotTRB : public CommandTRB {
                static EnableSlotTRB Create(bool cycle, uint8_t slot_type);
            };

            struct AddressDeviceTRB : public CommandTRB {
                static AddressDeviceTRB Create(bool cycle, bool bsr, uint8_t slot_id, const void* context_pointer);
            };

            struct LinkTRB : public CommandTRB {
                static LinkTRB Create(bool cycle, TRB* next);
            };

            struct TransferTRB : public TRB {
            protected:
                void SetDataBufferPointer(const void* pointer);
                void SetRawImmediateData(uint64_t data);
                void SetTRBTransferLength(uint16_t length);
                void SetTDSize(uint8_t size);
                void SetInterrupterTarget(uint16_t target);
                using TRB::SetCycle;
                void SetENT(bool ent);
                void SetISP(bool isp);
                void SetNoSnoop(bool no_snoop);
                void SetChain(bool chain);
                void SetInterruptOnCompletion(bool ioc);
                void SetImmediateData(bool immediate_data);
                void SetBEI(bool bei);
                using TRB::SetTRBType;
            };
        }
    }
}