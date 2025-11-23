#pragma once

#include <cstdint>

namespace PCI {
    struct Capability {
        uint8_t ID;
        uint8_t NextPointer;
    };

    class Interface {
    private:
        struct Offsets {
            static constexpr uint64_t VendorID              = 0x00;
            static constexpr uint64_t DeviceID              = 0x02;
            static constexpr uint64_t Command               = 0x04;
            static constexpr uint64_t Status                = 0x06;
            static constexpr uint64_t RevisionID            = 0x08;
            static constexpr uint64_t ClassCode             = 0x09;
            static constexpr uint64_t CacheLineSize         = 0x0C;
            static constexpr uint64_t LatencyTimer          = 0x0D;
            static constexpr uint64_t HeaderType            = 0x0E;
            static constexpr uint64_t BIST                  = 0x0F;
            static constexpr uint64_t CapabilitiesPointer   = 0x34;
            static constexpr uint64_t InterruptLine         = 0x3C;
            static constexpr uint64_t InterruptPin          = 0x3D;
        };

        struct CommandsMasks {
            static constexpr uint16_t IO        = 0x0001;
            static constexpr uint16_t MMIO      = 0x0002;
            static constexpr uint16_t BusMaster = 0x0004;
        };

        static constexpr uint32_t BAR_IO_FLAG       = 0x00000001;
        static constexpr uint32_t BAR_SIZE_MASK     = 0x00000006;
        static constexpr uint32_t BAR_32_FLAG       = 0x00000000;
        static constexpr uint32_t BAR_64_FLAG       = 0x00000004;
        static constexpr uint64_t BAR_MEM_ADDR_MASK = 0xFFFFFFFFFFFFFFF0;
        static constexpr uint64_t BAR_IO_ADDR_MASK  = 0xFFFFFFFFFFFFFFFC;
    
    protected:
        const uint8_t bus;
        const uint8_t device;
        const uint8_t function;

        uint8_t* const base;

        static void* MapMemoryBAR(volatile uint32_t* bar, uint64_t flags);
        static void* MapMemoryXBAR(volatile uint64_t* xbar, uint64_t flags);

        static void UnmapMemoryBAR(volatile uint32_t* bar, void* ptr);
        static void UnmapMemoryXBAR(volatile uint64_t* bar, void* ptr);

    public:
        volatile uint16_t* const VendorID;
        volatile uint16_t* const DeviceID;
        volatile uint16_t* const Command;
        volatile uint16_t* const Status;
        volatile uint8_t* const RevisionID;
        volatile struct {
            uint8_t ProgrammingInterface;
            uint8_t SubCode;
            uint8_t BaseCode;
        } *const ClassCode;
        volatile uint8_t* const CacheLineSize;
        volatile uint8_t* const LatencyTimer;
        volatile uint8_t* const HeaderType;
        volatile uint8_t* const BIST;
        volatile uint8_t* const CapabilitiesPointer;
        volatile uint8_t* const InterruptLine;
        volatile uint8_t* const InterruptPin;

        Interface(uint8_t bus, uint8_t device, uint8_t function, void* ptr);

        volatile uint8_t* GetBase() const;

        void EnableIO() const;
        void DisableIO() const;
        
        void EnableMMIO() const;
        void DisableMMIO() const;
        
        void EnableBusMaster() const;
        void DisableBusMaster() const;

        Capability* FindCapability(uint8_t id) const;
    };

    class IType0 : public Interface {
    private:
        struct Offsets {
            static constexpr uint64_t BARs                      = 0x10;
            static constexpr uint64_t CardbusCISPointer         = 0x28;
            static constexpr uint64_t SubsystemVendorID         = 0x2C;
            static constexpr uint64_t SubsystemID               = 0x2E;
            static constexpr uint64_t ExpansionROMBaseAddress   = 0x30;
            static constexpr uint64_t MinGnt                    = 0x3E;
            static constexpr uint64_t MaxGnt                    = 0x3F;
        };

        volatile uint32_t* const bar_base;
        volatile uint64_t* const xbar_base;
        
    public:
        volatile uint32_t* const CardbusCISPointer;
        volatile uint16_t* const SubsystemVendorID;
        volatile uint16_t* const SubsystemID;
        volatile uint32_t* const ExpansionROMBaseAddress;
        volatile uint8_t* const MinGnt;
        volatile uint8_t* const MaxGnt;

        IType0(const Interface& i);

        uint32_t ReadBAR(uint8_t id) const;
        void WriteBAR(uint8_t id, uint32_t value) const;

        uint64_t ReadXBAR(uint8_t id) const;
        void WriteXBAR(uint8_t id, uint64_t value) const;

        void* MapMemoryBAR(uint8_t id, uint64_t flags) const;
        void* MapMemoryXBAR(uint8_t id, uint64_t flags) const;

        void UnmapMemoryBAR(uint8_t id, void* ptr) const;
        void UnmapMemoryXBAR(uint8_t id, void* ptr) const;
    };

    class IType1 : public Interface {
    private:
        struct Offsets {
            static constexpr uint64_t BARs                      = 0x10;
            static constexpr uint64_t PrimaryBusNumber          = 0x18;
            static constexpr uint64_t SecondaryBusNumber        = 0x19;
            static constexpr uint64_t SubordinateBusNumber      = 0x1A;
            static constexpr uint64_t SecondaryLatencyTimer     = 0x1B;
            static constexpr uint64_t IOBase                    = 0x1C;
            static constexpr uint64_t IOLimit                   = 0x1D;
            static constexpr uint64_t SecondaryStatus           = 0x1E;
            static constexpr uint64_t MemoryBase                = 0x20;
            static constexpr uint64_t MemoryLimit               = 0x22;
            static constexpr uint64_t PrefetchableMemoryBase    = 0x24;
            static constexpr uint64_t PrefetchableMemoryLimit   = 0x26;
            static constexpr uint64_t PrefetchableBaseUpper32   = 0x28;
            static constexpr uint64_t PrefetchableLimitUpper32  = 0x2C;
            static constexpr uint64_t IOBaseUpper16             = 0x30;
            static constexpr uint64_t IOLimitUpper16            = 0x32;
            static constexpr uint64_t ExpansionROMBaseAddress   = 0x38;
            static constexpr uint64_t BridgeControl             = 0x3E;
        };
        
        volatile uint32_t* const bar_base;
        volatile uint64_t* const xbar;
    
    public:
        volatile uint8_t* const PrimaryBusNumber;
        volatile uint8_t* const SecondaryBusNumber;
        volatile uint8_t* const SubordinateBusNumber;
        volatile uint8_t* const SecondaryLatencyTimer;
        volatile uint8_t* const IOBase;
        volatile uint8_t* const IOLimit;
        volatile uint16_t* const SecondaryStatus;
        volatile uint16_t* const MemoryBase;
        volatile uint16_t* const MemoryLimit;
        volatile uint16_t* const PrefetchableMemoryBase;
        volatile uint16_t* const PrefetchableMemoryLimit;
        volatile uint32_t* const PrefetchableBaseUpper32;
        volatile uint32_t* const PrefetchableLimitUpper32;
        volatile uint16_t* const IOBaseUpper16;
        volatile uint16_t* const IOLimitUpper16;
        volatile uint32_t* const ExpansionROMBaseAddress;
        volatile uint16_t* const BridgeControl;

        IType1(const Interface& i);

        uint32_t ReadBAR(uint8_t id) const;
        void WriteBAR(uint8_t id, uint32_t value) const;

        uint64_t ReadXBAR() const;
        void WriteXBAR(uint64_t value) const;

        void* MapMemoryBAR(uint8_t id, uint64_t flags) const;
        void* MapMemoryXBAR(uint64_t flags) const;

        void UnmapMemoryBAR(uint8_t id, void* ptr) const;
        void UnmapMemoryXBAR(void* ptr) const;
    };
}
