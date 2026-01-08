#pragma once

#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/Response.hpp>

#include <devices/USB/xHCI/TRB.hpp>

namespace Devices {
    namespace USB {
        namespace xHCI {
            class PortSpeed {
            public:
                enum : uint8_t {
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

                operator decltype(InvalidSpeed) () const;

                static PortSpeed FromSpeedID(uint8_t id);
                uint8_t ToSpeedID() const;

                bool operator==(const decltype(InvalidSpeed)& speed) const;
                bool operator!=(const decltype(InvalidSpeed)& speed) const;
            };

            class SlotState {
            public:
                enum : uint8_t {
                    Invalid,
                    DisabledEnabled,
                    Default,
                    Addressed,
                    Configured
                };

                decltype(Default) value;

                SlotState(const decltype(Invalid)& state);
                operator decltype(Invalid) () const;

                static SlotState FromSlotState(uint8_t state);

                bool operator==(const decltype(Default)& state) const;
                bool operator!=(const decltype(Default)& state) const;
            };

            struct Context {
            protected:
                uint32_t data[8];

            public:
                void Reset();
            };

            template<class Base>
            struct ContextEx : public Base {
                uint32_t extended_data[8];

                inline void Reset() {
                    Base::Reset();
                    extended_data[0] = 0;
                    extended_data[1] = 0;
                    extended_data[2] = 0;
                    extended_data[3] = 0;
                    extended_data[4] = 0;
                    extended_data[5] = 0;
                    extended_data[6] = 0;
                    extended_data[7] = 0;
                }
            };

            struct SlotContext : public Context {
            private:
                static constexpr uint32_t   ROUTE_STRING_MASK       = 0x000FFFFF;
                static constexpr uint32_t   PORT_SPEED_MASK         = 0x00F00000;
                static constexpr uint8_t    PORT_SPEED_SHIFT        = 20;
                static constexpr uint32_t   CONTEXT_ENTRIES_MASK    = 0xF8000000;
                static constexpr uint8_t    CONTEXT_ENTRIES_SHIFT   = 27;
                static constexpr uint32_t   ROOT_HUB_PORT_MASK      = 0x00FF0000;
                static constexpr uint8_t    ROOT_HUB_PORT_SHIFT     = 16;
                static constexpr uint32_t   SLOT_STATE_MASK         = 0xF8000000;
                static constexpr uint8_t    SLOT_STATE_SHIFT        = 27;

            public:
                uint32_t GetRouteString() const;
                void SetRouteString(uint32_t route);

                PortSpeed GetPortSpeed() const;
                void SetPortSpeed(const PortSpeed& speed);

                bool GetMTT() const = delete;
                void SetMTT(bool enabled) = delete;

                bool GetHub() const = delete;
                void SetHub(bool enabled) = delete;

                uint8_t GetContextEntries() const;
                void SetContextEntries(uint8_t count);

                uint16_t GetMaxExitLatency() const = delete;
                void SetMaxExitLatency(uint16_t latency) = delete;

                uint8_t GetRootHubPort() const;
                void SetRootHubPort(uint8_t port);

                uint8_t GetPortsNumber() const = delete;
                void SetPortsNumber(uint8_t count) = delete;

                uint8_t GetParentHubSlotID() const = delete;
                void SetParentHubSlotID(uint8_t slot) = delete;

                uint8_t GetParentPort() const = delete;
                void SetParentPort(uint8_t port) = delete;

                uint8_t GetTTT() const = delete;
                void SetTTT(uint8_t time) = delete;

                uint16_t GetInterrupterTarget() const = delete;
                void SetInterrupterTarget(uint16_t target) = delete;

                uint8_t GetUSBAddress() const = delete;
                void ResetUSBAddress() = delete;

                SlotState GetSlotState() const;
                void ResetSlotState(); 
            };

            struct SlotContextEx : public ContextEx<SlotContext> {
            public:
                using ContextEx::Reset;
            };

            class EndpointState {
            public:
                enum : uint8_t {
                    Invalid,
                    Disabled,
                    Running,
                    Halted,
                    Stopped,
                    Error,
                    Reserved
                };

                decltype(Invalid) value;

                EndpointState(const decltype(Invalid)& state);
                operator decltype(Invalid) () const;

                static EndpointState FromEndpointState(uint8_t state);

                bool operator==(const decltype(Invalid)& state) const;
                bool operator!=(const decltype(Invalid)& state) const;
            };

            class EndpointType {
            public:
                enum : uint8_t {
                    Invalid,
                    IsochronousOut,
                    BulkOut,
                    InterruptOut,
                    ControlBidirectional,
                    IsochronousIn,
                    BulkIn,
                    InterruptIn
                };

                decltype(Invalid) value;

                EndpointType(const decltype(Invalid)& type);
                operator decltype(Invalid) () const;

                static EndpointType FromEndpointType(uint8_t type);
                uint8_t ToEndpointType() const;

                bool operator==(const decltype(Invalid)& type) const;
                bool operator!=(const decltype(Invalid)& type) const;
            };

            struct EndpointContext : public Context {
            private:
                static constexpr uint32_t   STATE_MASK                  = 0x00000007;
                static constexpr uint8_t    STATE_SHIFT                 = 0;
                static constexpr uint32_t   MULT_MASK                   = 0x00000300;
                static constexpr uint8_t    MULT_SHIFT                  = 8;
                static constexpr uint32_t   MAX_PS_STREAMS_MASK         = 0x00007C00;
                static constexpr uint8_t    MAX_PS_STREAMS_SHIFT        = 10;
                static constexpr uint32_t   INTERVAL_MASK               = 0x00FF0000;
                static constexpr uint8_t    INTERVAL_SHIFT              = 16;
                static constexpr uint32_t   CERR_MASK                   = 0x00000006;
                static constexpr uint8_t    CERR_SHIFT                  = 1;
                static constexpr uint32_t   ENDPOINT_TYPE_MASK          = 0x00000038;
                static constexpr uint8_t    ENDPOINT_TYPE_SHIFT         = 3;
                static constexpr uint32_t   MAX_PACKET_SIZE_MASK        = 0xFFFF0000;
                static constexpr uint8_t    MAX_PACKET_SIZE_SHIFT       = 16;
                static constexpr uint32_t   MAX_BURST_SIZE_MASK         = 0x0000FF00;
                static constexpr uint8_t    MAX_BURST_SIZE_SHIFT        = 8;
                static constexpr uint32_t   DCS_MASK                    = 0x00000001;
                static constexpr uint8_t    DCS_SHIFT                   = 0;
                static constexpr uint32_t   TR_DEQUEUE_POINTER_LO_MASK  = 0xFFFFFFF0;
                static constexpr uint32_t   AVERAGE_TRB_LENGTH_MASK     = 0x0000FFFF;
                static constexpr uint8_t    AVERAGE_TRB_LENGTH_SHIFT    = 0;

            public:
                EndpointState GetEndpointState() const;
                
                uint8_t GetMult() const;
                void SetMult(uint8_t mult);

                uint8_t GetMaxPStreams() const;
                void SetMaxPStreams(uint8_t streams);

                bool GetLSA() const = delete;
                void SetLSA(bool lsa) = delete;

                uint8_t GetInterval() const;
                void SetInterval(uint8_t interval);

                uint8_t GetMaxESITPayload() const = delete;
                void SetMaxESITPayload(uint8_t payload) = delete;

                uint8_t GetErrorCount() const;
                void SetErrorCount(uint8_t count);

                EndpointType GetEndpointType() const;
                void SetEndpointType(const EndpointType& type);

                bool GetHID() const = delete;
                void SetHID(bool hid) = delete;

                uint8_t GetMaxBurstSize() const;
                void SetMaxBurstSize(uint8_t size);

                uint16_t GetMaxPacketSize() const;
                void SetMaxPacketSize(uint16_t size);

                bool GetDCS() const;
                void SetDCS(bool dcs);

                const TransferTRB* GetTRDequeuePointer() const;
                void SetTRDequeuePointer(const TransferTRB* pointer);

                uint16_t GetAverageTRBLength() const;
                void SetAverageTRBLength(uint16_t length);
            };

            struct EndpointContextEx : public ContextEx<EndpointContext> {
            public:
                using ContextEx::Reset;
            };

            struct EndpointContextPair {
            public:
                EndpointContext out;
                EndpointContext in;
            };

            struct EndpointContextPairEx {
            public:
                EndpointContextEx out;
                EndpointContextEx in;
            };

            struct InputControlContext : public Context {
            public:
                void SetDropContext(uint8_t id);
                void SetAddContext(uint8_t id);
                void SetConfigurationValue(uint8_t config);
                void SetInterfaceNumber(uint8_t id);
                void SetAlternateSetting(uint8_t v);
            };

            struct InputControlContextEx : public ContextEx<InputControlContext> {
            public:
                using ContextEx::Reset;
            };

            struct OutputDeviceContext {
            public:
                SlotContext slot;
                EndpointContext control_endpoint;
                EndpointContextPair endpoints[15];
            };

            struct OutputDeviceContextEx {
            public:
                SlotContextEx slot;
                EndpointContextEx control_endpoint;
                EndpointContextPairEx endpoints[15];
            };

            struct InputDeviceContext {
            public:
                InputControlContext input_control;
                SlotContext slot;
                EndpointContext control_endpoint;
                EndpointContextPair endpoints[15];
            };

            struct InputDeviceContextEx {
            public:
                InputControlContextEx input_control;
                SlotContextEx slot;
                EndpointContextEx control_endpoint;
                EndpointContextPairEx endpoints[15];
            };

            class ContextWrapper {
            public:
                virtual void* GetInputDeviceContextAddress() const = 0;
                virtual void* GetOutputDeviceContextAddress() const = 0;

                virtual InputControlContext* GetInputControlContext() = 0;
                virtual SlotContext* GetSlotContext(bool is_in) = 0;
                virtual EndpointContext* GetControlEndpointContext(bool is_in) = 0;
                virtual EndpointContext* GetInputEndpointContext(uint8_t id, bool is_in) = 0;
                virtual EndpointContext* GetOutputEndpointContext(uint8_t id, bool is_in) = 0;

                virtual void ResetInputControl() = 0;
                virtual void ResetSlot() = 0;
                virtual void ResetControlEndpoint() = 0;
                virtual void ResetEndpoint(uint8_t id, bool is_in) = 0;
                virtual void Reset() = 0;

                static Optional<ContextWrapper*> Create(bool extended);
                virtual void Release() = 0;
            };

            class ContextWrapperBasic final : public ContextWrapper {
            private:
                ContextWrapperBasic(OutputDeviceContext* out, InputDeviceContext* in);

                OutputDeviceContext* const output;
                InputDeviceContext* const input;

            public:
                virtual void* GetInputDeviceContextAddress() const;
                virtual void* GetOutputDeviceContextAddress() const;

                virtual InputControlContext* GetInputControlContext();
                virtual SlotContext* GetSlotContext(bool is_in);
                virtual EndpointContext* GetControlEndpointContext(bool is_in);
                virtual EndpointContext* GetInputEndpointContext(uint8_t id, bool is_in);
                virtual EndpointContext* GetOutputEndpointContext(uint8_t id, bool is_in);

                virtual void ResetInputControl();
                virtual void ResetSlot();
                virtual void ResetControlEndpoint();
                virtual void ResetEndpoint(uint8_t id, bool is_in);
                virtual void Reset();

                static Optional<ContextWrapper*> Create();
                void Release();
            };

            class ContextWrapperEx final : public ContextWrapper {
            private:
                ContextWrapperEx(OutputDeviceContextEx* out, InputDeviceContextEx* in);

                OutputDeviceContextEx* const output;
                InputDeviceContextEx* const input;

            public:
                virtual void* GetInputDeviceContextAddress() const;
                virtual void* GetOutputDeviceContextAddress() const;

                virtual InputControlContext* GetInputControlContext();
                virtual SlotContext* GetSlotContext(bool is_in);
                virtual EndpointContext* GetControlEndpointContext(bool is_in);
                virtual EndpointContext* GetInputEndpointContext(uint8_t id, bool is_in);
                virtual EndpointContext* GetOutputEndpointContext(uint8_t id, bool is_in);

                virtual void ResetInputControl();
                virtual void ResetSlot();
                virtual void ResetControlEndpoint();
                virtual void ResetEndpoint(uint8_t id, bool is_in);
                virtual void Reset();

                static Optional<ContextWrapper*> Create();
                void Release();
            };

            class TransferRing {
            private:
                Utils::Lock lock;
                TransferTRB* const base;
                size_t index;
                size_t capacity;
                bool cycle = false;

                TransferRing(TransferTRB* base, size_t capacity);

                const TRB* EnqueueTRB(const TRB& trb);
                void UpdatePointer();

            public:
                static Optional<TransferRing*> Create(size_t pages);

                const TransferTRB* GetBase() const;
                void Release();
                bool GetCycle() const;
                const TRB* Enqueue(const TransferTRB& trb);
            };
        }
    }
}
