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
                void SetCycle(bool cycle);
                void SetTRBType(uint8_t type);
                void SetSlotType(uint8_t type);
            };

            struct NoOpTRB : public CommandTRB {
                static NoOpTRB Create(bool cycle);
            };

            struct EnableSlotTRB : public CommandTRB {
                static EnableSlotTRB Create(bool cycle, uint8_t slot_type);
            };

            struct LinkTRB : public CommandTRB {
                static LinkTRB Create(bool cycle, TRB* next);
            };

            struct TransferTRB : public TRB {

            };
        }
    }
}