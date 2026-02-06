// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

#pragma once

#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/Response.hpp>
#include <shared/SimpleAtomic.hpp>

#include <devices/USB/xHCI/Specification.hpp>

namespace Devices {
    namespace USB {
        class Driver;

        namespace xHCI {
            class Controller;

            struct DeviceInformation {
                uint32_t route_string;
                uint8_t parent_port;
                uint8_t root_hub_port;
                uint8_t slot_id;
                PortSpeed port_speed;
                uint8_t depth;
            };

            class Device {
            public:
                struct DeviceDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 18;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 1;

                    uint8_t protocolVersionMajor        = 0;
                    uint8_t protocolVersionMinor        = 0;
                    uint8_t deviceClass                 = 0;
                    uint8_t deviceSubClass              = 0;
                    uint8_t deviceProtocol              = 0;
                    uint16_t maxControlPacketSize       = 0;
                    uint16_t vendorID                   = 0;
                    uint16_t productID                  = 0;
                    uint8_t deviceVersionMajor          = 0;
                    uint8_t deviceVersionMinor          = 0;
                    uint8_t manufacturerDescriptorIndex = 0;
                    uint8_t productDescriptorIndex      = 0;
                    uint8_t serialNumberDescriptorIndex = 0;
                    uint8_t configurationsNumber        = 0;
                };

                struct RawDescriptorHeader {
                    uint8_t length;
                    uint8_t descriptorType;
                };

                struct DeviceSpecificDescriptor {
                    DeviceSpecificDescriptor* next  = nullptr;
                    uint8_t length                  = 0;
                    uint8_t descriptorType          = 0;
                    uint8_t data[];
                };

                struct EndpointDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 7;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 5;

                    enum class InterruptUsage : uint8_t {
                        Periodic,
                        Notification
                    };

                    enum class IsochronousSynchronization : uint8_t {
                        None,
                        Asynchronous,
                        Adaptive,
                        Synchronous
                    };

                    enum class IsochronousUsage : uint8_t {
                        Data,
                        Feedback,
                        ImplicitFeedback
                    };

                    uint8_t endpointAddress         = 0;
                    EndpointType endpointType       = EndpointType::Invalid;

                    typedef union {
                        InterruptUsage interruptUsage;
                        struct {
                            IsochronousSynchronization  isochSync;
                            IsochronousUsage            isochUsage;
                        };
                    } ExtraConfig;

                    ExtraConfig extraConfig;

                    uint16_t maxPacketSize  = 0;
                    uint8_t interval        = 0;

                    typedef struct {
                        bool valid;
                        uint8_t maxBurst;
                        uint32_t bytesPerInterval;
                        union {
                            uint32_t maxStreams;
                            uint32_t maxPacketsPerInterval;
                        };
                    } SuperSpeedConfig;

                    SuperSpeedConfig superSpeedConfig;
                    
                    void Release();
                };

                struct InterfaceDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 9;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 4;

                    uint8_t interfaceNumber             = 0;
                    uint8_t alternateSetting            = 0;
                    uint8_t endpointsNumber             = 0;
                    uint8_t interfaceDescriptorIndex    = 0;
                    EndpointDescriptor* endpoints       = nullptr;
                    DeviceSpecificDescriptor* extra     = nullptr;
                    InterfaceDescriptor* nextAlternate  = nullptr;
                    InterfaceDescriptor* next           = nullptr;

                    void AddExtra(DeviceSpecificDescriptor* descriptor);
                    Optional<DeviceSpecificDescriptor*> GetExtra(uint8_t type);
                    Success AddAlternate(const InterfaceDescriptor& alternate);
                    void Release();
                };

                struct InterfaceWrapper {
                    uint8_t interfaceClass              = 0;
                    uint8_t interfaceSubClass           = 0;
                    uint8_t interfaceProtocol           = 0;
                    InterfaceDescriptor descriptor;
                };

                struct FunctionDescriptor {
                    uint8_t functionClass               = 0;
                    uint8_t functionSubClass            = 0;
                    uint8_t functionProtocol            = 0;
                    uint8_t functionDescriptorIndex     = 0;
                    uint8_t interfacesNumber            = 0;
                    InterfaceDescriptor* interfaces     = nullptr;
                    FunctionDescriptor* next            = nullptr;

                    Success AddInterface(const InterfaceDescriptor& interface);
                    InterfaceDescriptor* GetInterface(uint8_t id);
                    void Release();
                };

                struct ConfigurationDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 9;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 2;

                    bool valid                              = false;
                    uint8_t interfacesNumber                = 0;
                    uint8_t configurationValue              = 0;
                    uint8_t configurationDescriptorIndex    = 0;
                    bool selfPowered                        = false;
                    bool supportsRemoteWakeup               = false;
                    uint8_t maxPower                        = 0;
                    uint8_t functionsNumber                 = 0;
                    
                    FunctionDescriptor* functions           = nullptr;

                    Optional<FunctionDescriptor*> AddFunction(const FunctionDescriptor& function);
                    FunctionDescriptor* GetFunction(uint8_t fClass, uint8_t fSubClass, uint8_t fProtocol) const;
                    void Release();
                };

                struct StringDescriptor {
                    static inline constexpr size_t DESCRIPTOR_TYPE = 3;
                    static inline constexpr size_t MIN_DESCRIPTOR_SIZE = 2;
                };

            private:
                Optional<uint16_t> GetDefaultMaxPacketSize() const;
                Success AddressDevice();

                Success FetchDeviceDescriptor();
                Success FetchConfigurations();
                Optional<ConfigurationDescriptor> ParseConfigurationDescriptor(uint8_t index);
                Optional<InterfaceWrapper> ParseInterfaceDescriptor(const uint8_t*& data, const uint8_t* limit);
                Optional<EndpointDescriptor> ParseEndpointDescriptor(const uint8_t*& data, const uint8_t* limit);
                Optional<DeviceSpecificDescriptor*> ParseDeviceSpecificDescriptor(const uint8_t*& data, const uint8_t* limit);

                Success InitiateTransfer(const TRB* trb, uint32_t reason);

                static size_t GetDescriptorSize(const void* data);
                static size_t GetDescriptorType(const void* data);

                template<typename T>
                static inline Success CheckDescriptor(const void* data) {
                    if (GetDescriptorSize(data) < T::DESCRIPTOR_SIZE || GetDescriptorType(data) != T::DESCRIPTOR_TYPE) {
                        return Failure();
                    }

                    return Success();
                }

                Controller& controller;
                const DeviceInformation information;

                ContextWrapper* context_wrapper = nullptr;
                TransferRing* control_transfer_ring = nullptr;

                Utils::Lock transfer_lock;
                Utils::SimpleAtomic<bool> transfer_complete{false};
                const TRB* awaiting_transfer = nullptr;
                TransferEventTRB transfer_result;

                DeviceDescriptor descriptor;
                ConfigurationDescriptor* configurations = nullptr;

                static constexpr size_t MAX_ENDPOINT_TRANSFER_RINGS = 15 * 2;
                TransferRing* endpoint_transfer_rings[MAX_ENDPOINT_TRANSFER_RINGS] = { nullptr };

                static Success SendRequest(
                    Device& device,
                    uint8_t bmRequestType,
                    uint8_t bRequest,
                    uint16_t wValue,
                    uint16_t wIndex,
                    uint16_t wLength,
                    uint8_t* buffer
                );

                Optional<uint8_t*> GetDescriptor(uint8_t type, uint8_t index, uint8_t languageID = 0);
                Success GetDescriptor(uint8_t type, uint8_t index, uint16_t length, uint8_t* buffer, uint8_t languageID = 0);
                Optional<char*> GetString(uint8_t index, uint16_t languageID = 0);

                static Success SetConfiguration(Device& device, uint8_t configuration_value);
                static Success ConfigureEndpoint(Device& device, const EndpointDescriptor& endpoint);
                TransferRing* GetEndpointTransferRing(uint8_t endpointAddress, bool input) const;

                static Optional<uint8_t> ConvertEndpointInterval(const Device& device, const EndpointType& type, uint16_t interval);
                
                Utils::Lock                 state_lock;
                Utils::SimpleAtomic<bool>   unavailable{false};
                Utils::SimpleAtomic<size_t> current_accesses{0};

                void            SetUnvailable();
                bool            IsUnavailable() const;
                Success         SetBusy();
                void            ReleaseBusy();
                bool            IsBusy() const;
                virtual void    Release();

                void RingDoorbell(uint8_t doorbellID) const;

                friend class USB::Driver;

                struct DriversNode {
                    static constexpr size_t MAX_DRIVERS = 7;

                    DriversNode* next = nullptr;
                    USB::Driver* drivers[MAX_DRIVERS] = { nullptr };
                };

                DriversNode* drivers = nullptr;

                Success AddDriver(USB::Driver* driver);
                Optional<USB::Driver*> FindDriverEvent(const TransferEventTRB& trb) const;
                void ReleaseDrivers();

            public:
                Device(Controller& controller, const DeviceInformation& information);

                const DeviceInformation& GetInformation() const;
                const void* GetOutputDeviceContext() const;

                Success Initialize();
                Success PostInitialization();
                void Destroy();

                void SignalTransferComplete(const TransferEventTRB& trb);

                const DeviceDescriptor& GetDeviceDescriptor() const;
            };
        }
    }
}
