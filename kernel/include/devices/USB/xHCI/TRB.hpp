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
                constexpr void SetCycle(bool cycle);
                constexpr void SetTRBType(uint8_t type);
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
                constexpr void SetSlotType(uint8_t type);
                constexpr void SetSlotID(uint8_t id);
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
                constexpr void SetDataBufferPointer(const void* pointer);
                constexpr void SetRawImmediateData(uint64_t data);
                constexpr void SetTRBTransferLength(uint32_t length);
                constexpr void SetTDSize(uint8_t size);
                constexpr void SetInterrupterTarget(uint16_t target);
                using TRB::SetCycle;
                constexpr void SetENT(bool ent);
                constexpr void SetISP(bool isp);
                constexpr void SetNoSnoop(bool no_snoop);
                constexpr void SetChain(bool chain);
                constexpr void SetInterruptOnCompletion(bool ioc);
                constexpr void SetImmediateData(bool immediate_data);
                constexpr void SetBEI(bool bei);
                using TRB::SetTRBType;
                constexpr void SetDirection(bool direction);
            };

            class TransferType {
            public:
                enum : uint8_t {
                    Invalid,
                    NoDataStage,
                    DataOutStage,
                    DataInStage
                };

                decltype(Invalid) value;

                constexpr operator decltype(Invalid) () const;

                static constexpr TransferType FromType(uint8_t type);
                constexpr uint8_t ToType() const;

                constexpr bool operator==(const decltype(Invalid)& type) const;
                constexpr bool operator!=(const decltype(Invalid)& type) const;
            };

            struct SetupTRB : public TransferTRB {
            private:
                constexpr void SetRequestType(uint8_t bmRequestType);
                constexpr void SetRequest(uint8_t bRequest);
                constexpr void SetValue(uint16_t wValue);
                constexpr void SetIndex(uint16_t wIndex);
                constexpr void SetLength(uint16_t wLength);
                constexpr void SetTransferType(const TransferType& type);
            
            public:                
                struct SetupDescriptor {
                    uint8_t bmRequestType;
                    uint8_t bRequest;
                    uint16_t wValue;
                    uint16_t wIndex;
                    uint16_t wLength;
                    uint32_t tranferLength;
                    uint16_t interrupterTarget;
                    bool cycle;
                    bool interruptOnCompletion;
                    TransferType transferType;
                };

                static SetupTRB Create(const SetupDescriptor& descriptor);
            };

            struct DataTRB : public TransferTRB {
                struct DataDescriptor {
                    void* bufferPointer;
                    uint32_t transferLength;
                    uint8_t tdSize;
                    uint16_t interrupterTarget;
                    bool cycle;
                    bool evaluateNextTRB;
                    bool interruptOnShortPacket;
                    bool noSnoop;
                    bool chain;
                    bool interruptOnCompletion;
                    bool immediateData;
                    bool direction;
                };

                static DataTRB Create(const DataDescriptor& descriptor);
            };

            struct StatusTRB : public TransferTRB {
                struct StatusDescriptor {
                    uint16_t interrupterTarget;
                    bool cycle;
                    bool evaluateNextTRB;
                    bool chain;
                    bool interruptOnCompletion;
                    bool direction;
                };

                static StatusTRB Create(const StatusDescriptor& descriptor);
            };
        }
    }
}