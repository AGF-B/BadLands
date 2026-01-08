#pragma once

#include <cstddef>
#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>
#include <devices/USB/xHCI/TRB.hpp>

namespace Devices {
    namespace USB {
        namespace HID {
            enum class DeviceClass {
                INVALID,
                KEYBOARD
            };

            class InterfaceDevice;

            class Device : public xHCI::Device {
            public:
                struct HIDDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 9;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 0x21;

                    uint8_t hidVersionMajor             = 0;
                    uint8_t hidVersionMinor             = 0;
                    uint8_t countryCode                 = 0;
                    uint8_t descriptorsNumber           = 0;
                    uint16_t reportDescriptorLength     = 0;
                };

            private:
                class HIDHierarchy {
                private:
                    struct Node {
                        struct Node* next = nullptr;
                        InterfaceDevice* device = nullptr;
                    };

                    Node* head = nullptr;
                    bool hasMultipleReports = false;
                    mutable size_t maxReportSize = 0;

                public:
                    inline HIDHierarchy() = default;

                    Success AddDevice(InterfaceDevice* device);
                    Optional<InterfaceDevice*> GetDevice(DeviceClass deviceClass) const;

                    inline void SignalMultipleReports() { hasMultipleReports = true; }
                    inline bool HasMultipleReports() const { return hasMultipleReports; }

                    size_t GetMaxReportSize() const;

                    void SendReport(uint8_t report_id, const uint8_t* data, size_t length);

                    void Release();
                };

                static inline constexpr uint8_t HID_REPORT_DESCRIPTOR_TYPE = 0x22;

                const FunctionDescriptor* function = nullptr;
                HIDHierarchy hierarchy;
                uint8_t interrupt_in_ep_address = 0;
                xHCI::TransferRing* endpoint_ring = nullptr;
                uint8_t* reportBuffer = nullptr;
                const xHCI::TRB* last_sent_trb = nullptr;

                Device(const xHCI::Device& device, const FunctionDescriptor* function, const HIDHierarchy& hierarchy, uint8_t* buffer);

                void InitiateTransaction();

                void SoftRelease();
                
                static Optional<HIDDescriptor> GetHIDDescriptor(InterfaceDescriptor* interface);
                static Optional<HIDDescriptor> ParseHIDDescriptor(const uint8_t* data, size_t length);

                void HandleTransactionComplete();

            public:
                class ReportDescriptor {
                public:
                    enum ItemType : uint8_t {
                        Main,
                        Global,
                        Local
                    };

                    struct Item {
                        ItemType type;
                        uint8_t tag;
                        uint32_t value;
                    };

                    ReportDescriptor(const uint8_t* data, size_t length);

                    Optional<Item> GetNextItem();

                private:
                    const uint8_t* const data;
                    const size_t length;
                    size_t position;
                };
            
                class ReportParser {
                public:
                    using Item = ReportDescriptor::Item;
                    using ItemType = ReportDescriptor::ItemType;

                    struct GlobalState {
                        uint32_t usagePage = 0;
                        uint32_t logicalMinimum = 0;
                        uint32_t logicalMaximum = 0;
                        uint32_t reportSize = 0;
                        uint32_t reportID = 0;
                        uint32_t reportCount = 0;
                    };

                    struct LocalState {
                        uint32_t usage = 0;
                        uint32_t usageMinimum = 0;
                        uint32_t usageMaximum = 0;
                    };

                    enum class GlobalEvent {
                        PAGE_CHANGED,
                        LOGICAL_MINIMUM_CHANGED,
                        LOGICAL_MAXIMUM_CHANGED,
                        REPORT_SIZE_CHANGED,
                        REPORT_ID_CHANGED,
                        REPORT_COUNT_CHANGED
                    };

                    inline ReportParser(ReportDescriptor& descriptor) : descriptor{descriptor} { }

                    inline const GlobalState& GetGlobalState() const {
                        return globalState;
                    }

                    inline const LocalState& GetLocalState() const {
                        return localState;
                    }

                    Optional<GlobalEvent> HandleGlobalItem(const Item& item, InterfaceDevice* device);
                    Success HandleLocalItem(const Item& item, InterfaceDevice* device);
                    Optional<HIDHierarchy> Parse();

                private:
                    static constexpr uint8_t GENERIC_DESKTOP_CONTROLS   = 0x1;

                    static constexpr uint8_t SUPPORTED_PAGES[] = {
                        GENERIC_DESKTOP_CONTROLS
                    };

                    static constexpr uint8_t GENERIC_KEYBOARD = 0x06;

                    static constexpr uint8_t SUPPORTED_GENERIC_DESKTOP_USAGES[] = {
                        GENERIC_KEYBOARD
                    };

                    static inline constexpr bool IsUsageSupported(uint32_t page, uint32_t usage) {
                        if (page == GENERIC_DESKTOP_CONTROLS) {
                            for (auto supportedUsage : SUPPORTED_GENERIC_DESKTOP_USAGES) {
                                if (usage == supportedUsage) {
                                    return true;
                                }
                            }
                        }

                        return false;
                    }

                    ReportDescriptor& descriptor;
                    GlobalState globalState;
                    LocalState localState;

                    bool hasMultipleReports = false;
                };

                static inline constexpr uint8_t GetClassCode() { return 0x03; }
                
                static Optional<Device*> Create(xHCI::Device& device, uint8_t configuration_value, const FunctionDescriptor* function);

                virtual Success PostInitialization() override;
                
                virtual void Release() override;

                virtual void SignalTransferComplete(const xHCI::TransferEventTRB& trb) override;
            };

            class InterfaceDevice {
            private:
                DeviceClass deviceClass;

            public:
                struct HIDState {
                    const Device::ReportParser::GlobalState& globalState;
                    const Device::ReportParser::LocalState& localState;
                };

                struct IOConfiguration {
                    bool constant;
                    bool variable;
                    bool relative;
                    bool wrap;
                    bool nonLinear;
                    bool noPreferred;
                    bool nullState;
                    bool _volatile;
                    bool bufferedBytes;
                };

                enum class CollectionType {
                    Physical,
                    Application,
                    Logical,
                    Report,
                    NamedArray,
                    UsageSwitch,
                    UsageModifier
                };

                InterfaceDevice(DeviceClass deviceClass) : deviceClass{deviceClass} { }

                virtual void Release() = 0;

                inline DeviceClass GetDeviceClass() const {
                    return deviceClass;
                }

                virtual bool IsUsageSupported(uint32_t page, uint32_t usage) = 0;
                virtual bool IsReportSupported(uint32_t reportID, bool input) = 0;

                virtual size_t GetMaxReportSize() const = 0;

                virtual Success AddInput(const HIDState& state, const IOConfiguration& config) = 0;
                virtual Success AddOutput(const HIDState& state, const IOConfiguration& config) = 0;
                virtual Success StartCollection(const HIDState& state, CollectionType type) = 0;
                virtual Success EndCollection() = 0;

                virtual void HandleReport(uint8_t report_id, const uint8_t* data, size_t length) = 0;
            };
        }
    }
}