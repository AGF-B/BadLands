#include <cstdint>

#include <type_traits>

#include <shared/memory/defs.hpp>

#include <mm/VirtualMemory.hpp>
#include <pci/Interface.hpp>

#include <screen/Log.hpp>

namespace PCI {
    Interface::Interface(uint8_t bus, uint8_t device, uint8_t function, void* ptr)
        : bus{bus}, device{device}, function{function},
        base{reinterpret_cast<uint8_t*>(ptr)},
        VendorID{reinterpret_cast<uint16_t*>(base + Offsets::VendorID)},
        DeviceID{reinterpret_cast<uint16_t*>(base + Offsets::DeviceID)},
        Command{reinterpret_cast<uint16_t*>(base + Offsets::Command)},
        Status{reinterpret_cast<uint16_t*>(base + Offsets::Status)},
        RevisionID{base + Offsets::RevisionID},
        ClassCode{reinterpret_cast<std::remove_const_t<decltype(ClassCode)>>(base + Offsets::ClassCode)},
        CacheLineSize{base + Offsets::CacheLineSize},
        LatencyTimer{base + Offsets::LatencyTimer},
        HeaderType{base + Offsets::HeaderType},
        BIST{base + Offsets::BIST},
        CapabilitiesPointer{base + Offsets::CapabilitiesPointer},
        InterruptLine{base + Offsets::InterruptLine},
        InterruptPin{base + Offsets::InterruptPin} {}

    volatile uint8_t* Interface::GetBase() const {
        return base;
    }

    void Interface::EnableIO() const {
        *Command |= CommandsMasks::IO;
    }

    void Interface::DisableIO() const {
        *Command &= ~CommandsMasks::IO;
    }

    void Interface::EnableMMIO() const {
        *Command |= CommandsMasks::MMIO;
    }

    void Interface::DisableMMIO() const {
        *Command &= ~CommandsMasks::MMIO;
    }

    void Interface::EnableBusMaster() const {
        *Command |= CommandsMasks::BusMaster;
    }

    void Interface::DisableBusMaster() const {
        *Command &= ~CommandsMasks::BusMaster;
    }

    Capability* Interface::FindCapability(uint8_t id) const {
        uint8_t offset = *CapabilitiesPointer & ~3;

        while (offset != 0) {
            Capability* cap = reinterpret_cast<Capability*>(base + offset);
            
            if (cap->ID == id) {
                return cap;
            }

            offset = cap->NextPointer;
        }

        return nullptr;
    }

    void* Interface::MapMemoryBAR(volatile uint32_t* bar, uint64_t flags) {
        const uint32_t bar_value = *bar;

        if (bar_value != 0 && (bar_value & BAR_IO_FLAG) == 0) {
            if ((bar_value & BAR_SIZE_MASK) == BAR_32_FLAG) {
                *bar = 0xFFFFFFFF;
                uint32_t size = ~*bar + 1;
                *bar = bar_value;

                void* address = reinterpret_cast<void*>(bar_value & BAR_MEM_ADDR_MASK);
                size_t pages = (size + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;

                return VirtualMemory::MapGeneralPages(address, pages, flags);
            }
        }

        return nullptr;
    }

    void* Interface::MapMemoryXBAR(volatile uint64_t* xbar, uint64_t flags) {
        auto* const lo_bar = reinterpret_cast<volatile uint32_t*>(xbar);
        auto* const hi_bar = lo_bar + 1;

        const uint64_t bar_value = ((uint64_t)*hi_bar << 32) | *lo_bar;

        if (bar_value != 0 && (bar_value & BAR_IO_FLAG) == 0) {
            if ((bar_value & BAR_SIZE_MASK) == BAR_64_FLAG) {
                *lo_bar = 0xFFFFFFFF;
                *hi_bar = 0xFFFFFFFF;

                uint64_t size = ~(((uint64_t)*hi_bar << 32) | *lo_bar) + 1;

                *lo_bar = static_cast<uint32_t>(bar_value);
                *hi_bar = static_cast<uint32_t>(bar_value >> 32);

                void* address = reinterpret_cast<void*>(bar_value & BAR_MEM_ADDR_MASK);
                size_t pages = (size + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;

                Log::printf("BAR size: 0x%0.16llx\n\r", size);

                return VirtualMemory::MapGeneralPages(address, pages, flags);
            }
        }

        return nullptr;
    }

    void Interface::UnmapMemoryBAR(volatile uint32_t* bar, void* ptr) {
        const uint32_t bar_value = *bar;

        if (bar_value != 0 && (bar_value & BAR_IO_FLAG) == 0) {
            if ((bar_value & BAR_SIZE_MASK) == BAR_32_FLAG) {
                *bar = 0xFFFFFFFF;
                uint32_t size = ~*bar + 1;
                *bar = bar_value;

                size_t pages = (size + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;

                VirtualMemory::UnmapGeneralPages(ptr, pages);
            }
        }
    }

    void Interface::UnmapMemoryXBAR(volatile uint64_t* xbar, void* ptr) {
        const uint64_t bar_value = *xbar;

        if (bar_value != 0 && (bar_value & BAR_IO_FLAG) == 0) {
            if ((bar_value & BAR_SIZE_MASK) == BAR_64_FLAG) {
                *xbar = 0xFFFFFFFFFFFFFFFF;
                uint64_t size = ~*xbar + 1;
                *xbar = bar_value;

                size_t pages = (size + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;

                VirtualMemory::MapGeneralPages(ptr, pages);
            }
        }
    }

    IType0::IType0(const Interface& i)
        : Interface(i),
        bar_base{reinterpret_cast<uint32_t*>(base + Offsets::BARs)},
        xbar_base{reinterpret_cast<uint64_t*>(base + Offsets::BARs)},
        CardbusCISPointer{reinterpret_cast<uint32_t*>(base + Offsets::CardbusCISPointer)},
        SubsystemVendorID{reinterpret_cast<uint16_t*>(base + Offsets::SubsystemVendorID)},
        SubsystemID{reinterpret_cast<uint16_t*>(base + Offsets::SubsystemID)},
        ExpansionROMBaseAddress{reinterpret_cast<uint32_t*>(base + Offsets::ExpansionROMBaseAddress)},
        MinGnt{base + Offsets::MinGnt},
        MaxGnt{base + Offsets::MaxGnt} {}

    uint32_t IType0::ReadBAR(uint8_t id) const {
        if (id < 6) {
            return bar_base[id];
        }

        return 0xFFFFFFFF;
    }

    void IType0::WriteBAR(uint8_t id, uint32_t value) const {
        if (id < 6) {
            bar_base[id] = value;
        }
    }

    uint64_t IType0::ReadXBAR(uint8_t id) const {
        if (id < 3) {
            return xbar_base[id];
        }

        return 0xFFFFFFFF;
    }

    void IType0::WriteXBAR(uint8_t id, uint64_t value) const {
        if (id < 3) {
            xbar_base[id] = value;
        }
    }

    void* IType0::MapMemoryBAR(uint8_t id, uint64_t flags) const {
        if (id < 6) {
            return Interface::MapMemoryBAR(&bar_base[id], flags);
        }

        return nullptr;
    }

    void* IType0::MapMemoryXBAR(uint8_t id, uint64_t flags) const {
        if (id < 3) {
            return Interface::MapMemoryXBAR(&xbar_base[id], flags);
        }

        return nullptr;
    }

    void IType0::UnmapMemoryBAR(uint8_t id, void* ptr) const {
        if (id < 6) {
            Interface::UnmapMemoryBAR(&bar_base[id], ptr);
        }
    }

    void IType0::UnmapMemoryXBAR(uint8_t id, void* ptr) const {
        if (id < 3) {
            Interface::UnmapMemoryXBAR(&xbar_base[id], ptr);
        }
    }


    IType1::IType1(const Interface& i)
        : Interface(i),
        bar_base{reinterpret_cast<uint32_t*>(base + Offsets::BARs)},
        xbar{reinterpret_cast<uint64_t*>(base + Offsets::BARs)},
        PrimaryBusNumber{base + Offsets::PrimaryBusNumber},
        SecondaryBusNumber{base + Offsets::SecondaryBusNumber},
        SubordinateBusNumber{base + Offsets::SubordinateBusNumber},
        SecondaryLatencyTimer{base + Offsets::SecondaryLatencyTimer},
        IOBase{base + Offsets::IOBase},
        IOLimit{base + Offsets::IOLimit},
        SecondaryStatus{reinterpret_cast<uint16_t*>(base + Offsets::SecondaryStatus)},
        MemoryBase{reinterpret_cast<uint16_t*>(base + Offsets::MemoryBase)},
        MemoryLimit{reinterpret_cast<uint16_t*>(base + Offsets::MemoryLimit)},
        PrefetchableMemoryBase{reinterpret_cast<uint16_t*>(base + Offsets::PrefetchableMemoryBase)},
        PrefetchableMemoryLimit{reinterpret_cast<uint16_t*>(base + Offsets::PrefetchableMemoryLimit)},
        PrefetchableBaseUpper32{reinterpret_cast<uint32_t*>(base + Offsets::PrefetchableBaseUpper32)},
        PrefetchableLimitUpper32{reinterpret_cast<uint32_t*>(base + Offsets::PrefetchableLimitUpper32)},
        IOBaseUpper16{reinterpret_cast<uint16_t*>(base + Offsets::IOBaseUpper16)},
        IOLimitUpper16{reinterpret_cast<uint16_t*>(base + Offsets::IOBaseUpper16)},
        ExpansionROMBaseAddress{reinterpret_cast<uint32_t*>(base + Offsets::ExpansionROMBaseAddress)},
        BridgeControl{reinterpret_cast<uint16_t*>(base + Offsets::BridgeControl)} {}
    
    uint32_t IType1::ReadBAR(uint8_t id) const {
        if (id < 2) {
            return bar_base[id];
        }

        return 0xFFFFFFFF;
    }

    void IType1::WriteBAR(uint8_t id, uint32_t value) const {
        if (id < 2) {
            bar_base[id] = value;
        }
    }

    uint64_t IType1::ReadXBAR() const {
        return *xbar;
    }

    void IType1::WriteXBAR(uint64_t value) const {
        *xbar = value;
    }

    void* IType1::MapMemoryBAR(uint8_t id, uint64_t flags) const {
        if (id < 2) {
            return Interface::MapMemoryBAR(&bar_base[id], flags);
        }

        return nullptr;
    }

    void* IType1::MapMemoryXBAR(uint64_t flags) const {
        return Interface::MapMemoryXBAR(xbar, flags);
    }

    void IType1::UnmapMemoryBAR(uint8_t id, void* ptr) const {
        if (id < 2) {
            Interface::UnmapMemoryBAR(&bar_base[id], ptr);
        }
    }

    void IType1::UnmapMemoryXBAR(void* ptr) const {
        Interface::UnmapMemoryXBAR(xbar, ptr);
    }
}
