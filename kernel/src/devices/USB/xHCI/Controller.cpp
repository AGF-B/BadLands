#include <cstdint>
#include <new>

#include <shared/memory/defs.hpp>
#include <shared/LockGuard.hpp>
#include <shared/MemoryOrdering.hpp>
#include <shared/Response.hpp>
#include <shared/SimpleAtomic.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/TRB.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/Panic.hpp>

#include <mm/Heap.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/Utils.hpp>

#include <pci/Interface.hpp>
#include <pci/MSI.hpp>

#include <sched/Self.hpp>
#include <sched/TaskContext.hpp>

#include <screen/Log.hpp>

namespace {
    static constexpr uint64_t max_timeout_ms = 1000;

    static constexpr uint8_t MMIO_BASE_BAR = 0;

    static constexpr uint64_t RING_SIZE = Shared::Memory::PAGE_SIZE;

    static constexpr uint32_t HCSPARAMS1_MAX_SLOTS_MASK = 0x000000FF;
    static constexpr uint32_t RTSOFF_MASK               = 0xFFFFFFE0;
    static constexpr uint32_t DBOFF_MASK                = 0xFFFFFFFC;
    static constexpr uint32_t USBCMD_RS_FLAG            = 0x00000001;
    static constexpr uint32_t USBCMD_HCRST_FLAG         = 0x00000002;
    static constexpr uint32_t USBCMD_INTE_FLAG          = 0x00000004;
    static constexpr uint32_t USBSTS_CNR_MASK           = 0x00000800;
    static constexpr uint32_t CONFIG_MAX_SLOTS_EN_MASK  = 0x000000FF;
    static constexpr uint32_t IMAN_IP_FLAG              = 0x00000001;
    static constexpr uint32_t IMAN_IE_FLAG              = 0x00000002;
    static constexpr uint64_t ERDP_EHB_FLAG             = 0x0000000000000008;

    extern "C" void USB_xHCI_CONTROLLER_IRQ_HANDLER();
}

namespace Devices::USB::xHCI {
    uint8_t Controller::OperationalPort::GetSpeedID() const {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000000F;
        return static_cast<uint8_t>((PORTSC >> SHIFT) & MASK);
    }    

    uint32_t Controller::ExtendedCapability::GetDWord(uint64_t i) const {
        return *(reinterpret_cast<const volatile uint32_t*>(this) + i);
    }

    uint8_t Controller::ExtendedCapability::GetByte(uint64_t i) const {
        return (GetDWord(i / sizeof(uint32_t)) >> (8 * (i % sizeof(uint32_t)))) & 0xFF;
    }

    uint8_t Controller::ExtendedCapability::GetID() const {
        return GetByte(0x00);
    }

    uint8_t Controller::ExtendedCapability::GetNextPointer() const {
        return GetByte(0x01);
    }

    uint8_t Controller::SupportedProtocolCapability::GetMinorRevision() const {
        return GetByte(0x02);
    }

    uint8_t Controller::SupportedProtocolCapability::GetMajorRevision() const {
        return GetByte(0x03);
    }

    uint32_t Controller::SupportedProtocolCapability::GetNameString() const {
        return GetDWord(0x01);
    }

    uint8_t Controller::SupportedProtocolCapability::GetCompatiblePortOffset() const {
        return GetByte(0x08);
    }

    uint8_t Controller::SupportedProtocolCapability::GetCompatiblePortCount() const {
        return GetByte(0x09);
    }

    uint16_t Controller::SupportedProtocolCapability::GetProtocolDefined() const {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x00000FFF;
        return static_cast<uint16_t>((GetDWord(0x2) >> SHIFT) & MASK);
    }

    uint8_t Controller::SupportedProtocolCapability::GetSpeedIDCount() const {
        static constexpr uint8_t SHIFT = 4;
        static constexpr uint8_t MASK = 0x0F;
        return (GetByte(0x0B) >> SHIFT) & MASK;
    }

    uint8_t Controller::SupportedProtocolCapability::GetProtocolSlotType() const {
        return GetByte(0x0C);
    }

    Controller::ERSTEntry Controller::ERSTEntry::Create(void* _ptr, uint16_t size) {
        const uint64_t ptr = reinterpret_cast<uint64_t>(_ptr);

        ERSTEntry entry;

        entry.data[0] = static_cast<uint32_t>(ptr & 0xFFFFFFC0);
        entry.data[1] = static_cast<uint32_t>(ptr >> 32);
        entry.data[2] = static_cast<uint32_t>(size);
        entry.data[3] = 0;

        return entry;
    }

    Controller::Controller(const PCI::Interface& interface) : Interrupts::InterruptProvider(), interface{interface} { }

    void Controller::HandleIRQ([[maybe_unused]] void* sp, [[maybe_unused]] uint64_t error_code) {
        static constexpr uint32_t USBSTS_EINT_MASK = 0x00000008;

        opregs->USBSTS = USBSTS_EINT_MASK;

        EventTRB* event = GetCurrentEvent();

        APIC::SendEOI();

        /// TODO: Implement special methods that buffer prints
        /// happening in interrupt handlers
        while (event->GetCycle() == event_cycle) {
            switch (event->GetType()) {
                case EventTRB::Type::TransferEvent:
                    Log::putsSafe("USB xHCI: Unhandled Transfer Event\n\r");
                    break;
                case EventTRB::Type::CommandCompletionEvent: {
                    command_completion.data[3] = event->data[3];
                    command_completion.data[1] = event->data[1];
                    command_completion.data[0] = event->data[0];
                    __blatomic_store_4(&command_completion.data[2], event->data[2], Utils::MemoryOrder::SEQ_CST);
                    break;
                }
                case EventTRB::Type::PortStatusChangeEvent: {
                    port_update = true;
                    ports[reinterpret_cast<PortStatusChangeEventTRB*>(event)->GetPortID() - 1].dirty = true;
                    Self().GetTaskManager().UnblockTask(port_updater_task_id);
                    break;
                }
                default: {
                    Log::putsSafe("USB xHCI: Unknown Event Type\n\r");
                    break;
                }
            }
            
            UpdateEventPointer();
            event = GetCurrentEvent();
        }

        GetPrimaryInterrupter()->ERDP = reinterpret_cast<uint64_t>(GetCurrentEvent()) | ERDP_EHB_FLAG;
    }

    void Controller::ReleaseResources() {
        Interrupts::ReleaseInterrupt(interrupt_vector);
        
        if (MMIO_base != nullptr) {
            DisablePrimaryInterrupter();
            DisableHostInterrupts();
            DisableHost();
            interface.DisableBusMaster();
        }

        ReleaseMMIO();
        ReleaseDCBAAP();
        ReleaseCommandRing();
        ReleaseEventRing();
        ReleasePorts();
        ReleaseScratchpad();
    }

    bool Controller::ConfigureMMIO() {
        static constexpr uint64_t BAR_MAPPING_FLAGS = 
            Shared::Memory::PTE_PRESENT
            | Shared::Memory::PTE_READWRITE
            | Shared::Memory::PTE_UNCACHEABLE;

        interface.DisableMMIO();

        MMIO_base = interface.MapMemoryXBAR(0, BAR_MAPPING_FLAGS);

        if (MMIO_base == nullptr) {
            return false;
        }

        interface.EnableMMIO();

        uint8_t* const base = reinterpret_cast<uint8_t*>(MMIO_base);

        capability = reinterpret_cast<CapabilityRegisters*>(base);
        opregs = reinterpret_cast<OperationalRegisters*>(base + capability->CAPLENGTH);
        primary_rtregs = reinterpret_cast<RuntimeRegisters*>(base + (capability->RTSOFF & RTSOFF_MASK));
        doorbell_regs = reinterpret_cast<DoorbellRegisters*>(base + (capability->DBOFF & DBOFF_MASK));

        return true;
    }

    void Controller::ReleaseMMIO() {
        if (MMIO_base != nullptr) {
            interface.UnmapMemoryXBAR(0, MMIO_base);
            MMIO_base = nullptr;
        }
    }

    void Controller::ConfigureMaxSlotsEnabled() {
        const uint8_t max_slots_available = capability->HCSPARAMS1 & HCSPARAMS1_MAX_SLOTS_MASK;

        max_slots_enabled = opregs->CONFIG & CONFIG_MAX_SLOTS_EN_MASK;

        if (max_slots_enabled != max_slots_available) {
            max_slots_enabled = max_slots_available;
            opregs->CONFIG |= max_slots_enabled & CONFIG_MAX_SLOTS_EN_MASK;
        }
    }

    void Controller::ConfigurePrimaryInterrupter() const {
        auto* const primary_interrupter = GetPrimaryInterrupter();
        
        primary_interrupter->ERSTSZ = (primary_interrupter->ERSTSZ & 0xFFFF0000)
            | static_cast<uint32_t>(GetEventRingSegments());
        primary_interrupter->ERDP = reinterpret_cast<uint64_t>(event_ring) | ERDP_EHB_FLAG;
        primary_interrupter->ERSTBA = reinterpret_cast<uint64_t>(event_ring) + GetEventRingSegmentTableOffset();
    }

    Controller::RuntimeRegisters::IRS* Controller::GetPrimaryInterrupter() const {
        return &primary_rtregs->InterrupterRegisterSets[0];
    }

    void Controller::EnablePrimaryInterrupter() const {
        GetPrimaryInterrupter()->IMAN |= IMAN_IP_FLAG | IMAN_IE_FLAG;
    }

    void Controller::DisablePrimaryInterrupter() const {
        GetPrimaryInterrupter()->IMAN &= ~(IMAN_IP_FLAG | IMAN_IE_FLAG);
    }

    size_t Controller::GetDCBAAPPages() const {
        static constexpr size_t DCBAA_ENTRY_SIZE = 8;

        return ((max_slots_enabled + 1) * DCBAA_ENTRY_SIZE + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;
    }

    bool Controller::ConfigureDCBAAP() {
        static constexpr uint64_t DCBAA_MAPPING_FLAGS =
            Shared::Memory::PTE_PRESENT
            | Shared::Memory::PTE_READWRITE
            | Shared::Memory::PTE_UNCACHEABLE;

        const size_t dcbaa_pages = GetDCBAAPPages();
        
        devices = static_cast<Device*>(Heap::Allocate(max_slots_enabled));

        if (devices == nullptr) {
            return false;
        }

        dcbaa = reinterpret_cast<DCBAA*>(VirtualMemory::AllocateDMA(dcbaa_pages));

        if (dcbaa == nullptr) {
            Heap::Free(devices);
            return false;
        }

        VirtualMemory::ChangeMappingFlags(dcbaa, DCBAA_MAPPING_FLAGS, dcbaa_pages);

        Utils::memset(dcbaa, 0, dcbaa_pages * Shared::Memory::PAGE_SIZE);

        const uint64_t dcbaap = reinterpret_cast<uint64_t>(dcbaa);

        opregs->DCBAAP_LO = static_cast<uint32_t>(dcbaap);
        opregs->DCBAAP_HI = static_cast<uint32_t>(dcbaap >> 32);

        return true;
    }

    void Controller::ReleaseDCBAAP() {
        if (dcbaa != nullptr) {
            VirtualMemory::FreeDMA(dcbaa, GetDCBAAPPages());
            dcbaa = nullptr;
            Heap::Free(devices);
            devices = nullptr;
        }
    }

    void Controller::WriteDCBAAEntry(uint8_t slot, void* ptr) const {
        if (slot <= max_slots_enabled && dcbaa != nullptr) {
            dcbaa[slot].ptr = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(ptr) & ~(uint64_t)0x3F);
        }
    }

    constexpr size_t Controller::GetCommandRingPages() {
        return (RING_SIZE + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;
    }

    bool Controller::ConfigureCommandRing() {
        static constexpr uint64_t COMMAND_RING_MAPPING_FLAGS =
            Shared::Memory::PTE_PRESENT
            | Shared::Memory::PTE_READWRITE
            | Shared::Memory::PTE_UNCACHEABLE;
        
        static constexpr size_t ring_pages = GetCommandRingPages();

        command_ring = reinterpret_cast<TRB*>(VirtualMemory::AllocateDMA(ring_pages));

        if (command_ring == nullptr) {
            return false;
        }

        VirtualMemory::ChangeMappingFlags(command_ring, COMMAND_RING_MAPPING_FLAGS, ring_pages);

        Utils::memset(command_ring, 0, RING_SIZE);

        command_capacity = RING_SIZE / sizeof(TRB);

        const uint64_t crp = reinterpret_cast<uint64_t>(command_ring);

        static constexpr uint32_t CRP_MASK = 0xFFFFFFC0;
        static constexpr uint32_t CRCR_RCS_FLAG = 0x00000001;

        uint32_t crcr_lo = opregs->CRCR_LO;
        crcr_lo = (crcr_lo & ~(CRP_MASK)) | static_cast<uint32_t>(crp & CRP_MASK);

        opregs->CRCR_LO = crcr_lo | CRCR_RCS_FLAG;
        opregs->CRCR_HI = static_cast<uint32_t>(crp >> 32);

        return true;
    }

    void Controller::ReleaseCommandRing() {
        if (command_ring != nullptr) {
            VirtualMemory::FreeDMA(command_ring, GetCommandRingPages());
            command_ring = nullptr;
        }
    }

    void Controller::UpdateCommandPointer() {
        if (++command_index >= command_capacity - 1) {
            LinkTRB link = LinkTRB::Create(GetCommandCycle(), command_ring);
            EnqueueCommand(link);            
            command_index = 0;
            command_cycle = !command_cycle;
        }
    }

    void Controller::EnqueueCommand(const CommandTRB& trb) const {
        command_ring[command_index] = trb;
    }

    void Controller::SignalCommand() const {
        doorbell_regs->r[0] = 0;
    }

    constexpr size_t Controller::GetEventRingPages() {
        return (RING_SIZE + sizeof(ERSTEntry) + Shared::Memory::PAGE_SIZE - 1) / Shared::Memory::PAGE_SIZE;
    }

    constexpr uint8_t Controller::GetEventRingSegments() {
        return 1;
    }

    constexpr uint64_t Controller::GetEventRingSegmentTableOffset() {
        constexpr uint64_t ERST_MEM_ALIGN = 0x40;
        return GetEventRingPages() * Shared::Memory::PAGE_SIZE - ERST_MEM_ALIGN;
    }

    bool Controller::ConfigureEventRing() {
        static constexpr uint64_t EVENT_RING_MAPPING_FLAGS =
            Shared::Memory::PTE_PRESENT
            | Shared::Memory::PTE_READWRITE
            | Shared::Memory::PTE_UNCACHEABLE;
        
        static constexpr size_t event_ring_pages = GetEventRingPages();

        event_ring = reinterpret_cast<EventTRB*>(VirtualMemory::AllocateDMA(event_ring_pages));

        if (event_ring == nullptr) {
            return false;
        }

        VirtualMemory::ChangeMappingFlags(event_ring, EVENT_RING_MAPPING_FLAGS, event_ring_pages);

        Utils::memset(event_ring, 0, event_ring_pages * Shared::Memory::PAGE_SIZE);

        static constexpr uint64_t ERST_OFF = GetEventRingSegmentTableOffset();
        static constexpr uint16_t EVENT_TRB_COUNT = ERST_OFF / sizeof(TRB);

        event_capacity = EVENT_TRB_COUNT;

        ERSTEntry* const erst = reinterpret_cast<ERSTEntry*>(reinterpret_cast<uint8_t*>(event_ring) + ERST_OFF);

        *erst = ERSTEntry::Create(event_ring, EVENT_TRB_COUNT);

        event_ring_segment_table = erst;

        return true;
    }

    void Controller::ReleaseEventRing() {
        if (event_ring != nullptr) {
            VirtualMemory::FreeDMA(event_ring, GetEventRingPages());
            event_ring = nullptr;
        }
    }

    void Controller::UpdateEventPointer() {
        if (++event_index >= event_capacity) {
            event_index = 0;
            event_cycle = !event_cycle;
        }
    }

    EventTRB* Controller::GetCurrentEvent() const {
        return &event_ring[event_index];
    }

    bool Controller::GetEventCycle() const {
        return event_cycle;
    }

    size_t Controller::GetScratchpadPages() const {
        return (max_scratchpad_buffers * sizeof(void*) + Shared::Memory::PAGE_SIZE - 1)
            / Shared::Memory::PAGE_SIZE;
    }

    bool Controller::ConfigureScratchpad() {
        static constexpr uint8_t HCSPARAMS2_MAX_SPB_LO_SHIFT = 27;
        static constexpr uint32_t HCSPARAMS2_MAX_SPB_LO_MASK = 0x1F;
        static constexpr uint8_t HCSPARAMS2_MAX_SPB_HI_SHIFT = 21;
        static constexpr uint32_t HCSPARAMS2_MAX_SPB_HI_MASK = 0x1F;

        // first check PAGESIZE = 4 KB
        // otherwise refuse to proceed if the controller needs at least one buffer

        const uint32_t hcsparams2 = capability->HCSPARAMS2;

        max_scratchpad_buffers =
            (((hcsparams2 >> HCSPARAMS2_MAX_SPB_HI_SHIFT) & HCSPARAMS2_MAX_SPB_HI_MASK) << 5)
            | ((hcsparams2 >> HCSPARAMS2_MAX_SPB_LO_SHIFT) & HCSPARAMS2_MAX_SPB_LO_MASK);


        if (max_scratchpad_buffers == 0) {
            return true;
        }
        else if ((opregs->PAGESIZE & 0x0000FFFF) != 1) {
            return false;
        }

        const size_t array_pages = GetScratchpadPages();

        // allocate scratchpad buffer array
        scratchpad_buffer_array = reinterpret_cast<void**>(VirtualMemory::AllocateDMA(array_pages));

        if (scratchpad_buffer_array == nullptr) {
            return false;
        }

        VirtualMemory::ChangeMappingFlags(
            scratchpad_buffer_array,
            Shared::Memory::PTE_PRESENT | Shared::Memory::PTE_READWRITE | Shared::Memory::PTE_UNCACHEABLE,
            array_pages
        );

        Utils::memset(scratchpad_buffer_array, 0, array_pages * Shared::Memory::PAGE_SIZE);

        // allocate individual scratchpad buffers
        for (uint16_t i = 0; i < max_scratchpad_buffers; ++i) {
            void* buffer = PhysicalMemory::Allocate();

            if (buffer == nullptr) {
                for (size_t j = 0; j < i; ++j) {
                    PhysicalMemory::Free(scratchpad_buffer_array[j]);
                }
                VirtualMemory::FreeDMA(scratchpad_buffer_array, array_pages);
                scratchpad_buffer_array = nullptr;
                return false;
            }

            // zero out the page
            auto* vbuffer = VirtualMemory::MapGeneralPages(
                buffer,
                1,
                Shared::Memory::PTE_PRESENT | Shared::Memory::PTE_READWRITE | Shared::Memory::PTE_UNCACHEABLE
            );

            if (vbuffer == nullptr) {
                for (size_t j = 0; j <= i; ++j) {
                    PhysicalMemory::Free(scratchpad_buffer_array[j]);
                }
                VirtualMemory::FreeDMA(scratchpad_buffer_array, array_pages);
                scratchpad_buffer_array = nullptr;
                return false;
            }

            Utils::memset(vbuffer, 0, Shared::Memory::PAGE_SIZE);

            VirtualMemory::UnmapGeneralPages(vbuffer, 1);

            scratchpad_buffer_array[i] = buffer;
        }

        // update the scratchpad buffer array pointer in the DCBAA slot 0
        WriteDCBAAEntry(0, scratchpad_buffer_array);

        return true;
    }

    void Controller::ReleaseScratchpad() {
        if (scratchpad_buffer_array != nullptr) {
            for (uint16_t i = 0; i < max_scratchpad_buffers; ++i) {
                if (scratchpad_buffer_array[i] != nullptr) {
                    PhysicalMemory::Free(scratchpad_buffer_array[i]);
                }
            }
            VirtualMemory::FreeDMA(scratchpad_buffer_array, GetScratchpadPages());
            scratchpad_buffer_array = nullptr;
        }
    }

    uint8_t Controller::GetMaxPorts() const {
        static constexpr uint8_t HCSPARAMS1_MAX_PORTS_SHIFT = 24;
        static constexpr uint32_t HCSPARAMS1_MAX_PORTS_MASK = 0x000000FF;
        return (capability->HCSPARAMS1 >> HCSPARAMS1_MAX_PORTS_SHIFT) & HCSPARAMS1_MAX_PORTS_MASK;
    }

    Controller::OperationalPort* Controller::AccessOperationalPort(size_t portid) const {
        static constexpr uint64_t PORTS_OFFSET = 0x400;

        if (portid < GetMaxPorts()) {
            return reinterpret_cast<OperationalPort*>(
                reinterpret_cast<uint8_t*>(opregs) + PORTS_OFFSET + portid * sizeof(OperationalPort)
            );
        }

        return nullptr;
    }

    bool Controller::ConfigurePortVersions() {
        const size_t vector_size = GetMaxPorts() * sizeof(Port);

        ports = reinterpret_cast<Port*>(Heap::Allocate(vector_size));

        if (ports == nullptr) {
            return false;
        }

        Utils::memset(ports, 0, vector_size);

        auto* supported_protocol = FindSupportedProtocolCapability();

        if (supported_protocol == nullptr) {
            // not possible on xHCI
            ReleasePorts();
            return false;
        }

        while (supported_protocol != nullptr) {
            const uint8_t offset = supported_protocol->GetCompatiblePortOffset();
            const uint8_t limit = offset + supported_protocol->GetCompatiblePortCount();
            const uint8_t major = supported_protocol->GetMajorRevision();

            for (uint8_t p = offset; p < limit; ++p) {
                auto& port = ports[p - 1];
                port.major = major;
                port.slot_type = supported_protocol->GetProtocolSlotType();
                port.dirty = true;
            }

            supported_protocol = reinterpret_cast<SupportedProtocolCapability*>(
                FindNextExtendedCapability(supported_protocol->GetID(), supported_protocol)
            );
        }

        return true;
    }

    void Controller::UpdatePortsTrampoline(Controller* controller) {
        controller->UpdatePorts();
    }

    void Controller::UpdatePorts() {
        while (true) {
            for (size_t i = 0; i < GetMaxPorts(); ++i) {
                if (ports[i].dirty) {
                    ports[i].dirty = false;
                    uint8_t revision = ports[i].major;

                    auto* const port = AccessOperationalPort(i);

                    if (!(port->PORTSC & OperationalPort::PORTSC_PP)) {
                        Log::printfSafe("[xHCI] Port 0x%0.2hhx (USB %hhu) is powered off\n\r", i, revision);
                        continue;
                    }

                    const bool finished_reset = port->PORTSC & OperationalPort::PORTSC_PRC;
                    const bool conn_changed   = port->PORTSC & OperationalPort::PORTSC_CSC;
                    const bool port_enabled   = port->PORTSC & OperationalPort::PORTSC_PED;
                    const bool is_connected   = port->PORTSC & OperationalPort::PORTSC_CCS;

                    // acknowledge and reset port flags
                    port->PORTSC = OperationalPort::PORTSC_CSC | OperationalPort::PORTSC_PRC | OperationalPort::PORTSC_PP;

                    if (!is_connected) {
                        Log::printfSafe("[xHCI] Port 0x%0.2hhx (USB %hhu) is disconnected\n\r", i, revision);
                        uint8_t& slot = ports[i].slot;

                        if (slot != 0) {
                            DisableSlot(slot);
                            slot = 0;
                        }

                        continue;
                    }

                    if (revision == 2) {
                        if (!(port_enabled && finished_reset)) {
                            if (conn_changed) {
                                // reset port
                                Log::printfSafe("[xHCI] Port 0x%0.2hhx (USB 2) reset in progress\n\r", i);
                                port->PORTSC = OperationalPort::PORTSC_PR | OperationalPort::PORTSC_PP;
                                continue;
                            }

                            Log::printfSafe("[xHCI] Port 0x%0.2hhx (USB 2) reset failed\n\r", i);
                            
                            continue;
                        }
                    }
                    else if (revision == 3) {
                        if (!conn_changed | !port_enabled) {
                            Log::printfSafe("[xHCI] Port 0x%0.2hhx (USB 3) reset failed\n\r", i);
                            continue;
                        }
                    }
                    else {
                        continue;
                    }

                    const uint8_t speed_id = port->GetSpeedID();
                    
                    Log::printfSafe(
                        "[xHCI] Device attached and connected to port 0x%0.2hhx (USB %hhu) with speed %u\n\r",
                        i,
                        ports[i].major,
                        speed_id
                    );

                    auto slot_id = EnableSlot(ports[i].slot_type);

                    if (slot_id < 0) {
                        Log::printfSafe("[xHCI] Could not enable slot for device ttached on port 0x%0.2hhx\n\r", i);
                        continue;
                    }
                    else {
                        ports[i].slot = static_cast<uint8_t>(slot_id);

                        Log::printfSafe("[xHCI] Port 0x%0.2hhx mapped to slot %hhu\n\r", i, ports[i].slot);

                        new (&devices[slot_id - 1]) Device(*this, DeviceDescriptor{
                            .route_string = static_cast<uint8_t>(i + i),
                            .parent_port = static_cast<uint8_t>(i + 1),
                            .slot_id = ports[i].slot,
                            .port_speed = PortSpeed::FromSpeedID(speed_id),
                            .depth = 0
                        });

                        if (!devices[slot_id - 1].Initialize().IsSuccess()) {
                            Log::printfSafe("[xHCI] Failed to initialize device on port 0x%0.2hhx\n\r", i);
                        }
                    }
                }
            }

            __asm__ volatile("cli");
            if (!port_update) {
                Self().GetTaskManager().BlockTask(port_updater_task_id);
            }
            port_update = false;
            __asm__ volatile("sti");
        }
    }

    void Controller::ReleasePorts() {
        if (ports != nullptr) {
            Heap::Free(ports);
            ports = nullptr;
        }
    }

    int16_t Controller::EnableSlot(uint8_t slot_type) {
        EnableSlotTRB trb = EnableSlotTRB::Create(GetCommandCycle(), slot_type);

        auto result = SendCommand(trb);

        if (!result.HasValue()) {
            return -1;
        }

        const auto slot_id = result.GetValue().GetSlotID();

        if (slot_id == 0) {
            return -1;
        }

        return slot_id;
    }

    void Controller::DisableSlot(uint8_t id) const {
        (void)id;
        Panic::PanicShutdown("UNIMPLEMENTED XHCI CONTROLLER METHOD: DisableSlot\n\r");
    }

    Controller::ExtendedCapability* Controller::FindExtendedCapability(uint8_t id) const {
        static constexpr uint32_t HCCPARAMS1_XECP_MASK = 0xFFFF0000;
        static constexpr uint8_t HCCPARAMS1_XECP_SHIFT = 16;

        const uint32_t xECP = (capability->HCCPARAMS1 & HCCPARAMS1_XECP_MASK) >> HCCPARAMS1_XECP_SHIFT;

        if (xECP == 0) {
            return nullptr;
        }

        uint32_t current_offset = xECP * sizeof(uint32_t);

        while (true) {
            ExtendedCapability* excap = reinterpret_cast<ExtendedCapability*>(
                reinterpret_cast<uint8_t*>(MMIO_base) + current_offset
            );

            if (excap->GetID() == id) {
                return excap;
            }

            if (excap->GetNextPointer() == 0) {
                return nullptr;
            }

            current_offset += static_cast<uint32_t>(excap->GetNextPointer()) * sizeof(uint32_t);
        }
    }

    Controller::ExtendedCapability* Controller::FindNextExtendedCapability(uint8_t id, ExtendedCapability* ptr) const {
        if (ptr == nullptr) {
            return FindExtendedCapability(id);
        }

        uint32_t current_offset = reinterpret_cast<uint8_t*>(ptr) - reinterpret_cast<uint8_t*>(MMIO_base);

        while (ptr->GetNextPointer() != 0) {
            current_offset += static_cast<uint32_t>(ptr->GetNextPointer()) * sizeof(uint32_t);

            ptr = reinterpret_cast<ExtendedCapability*>(
                reinterpret_cast<uint8_t*>(MMIO_base) + current_offset
            );

            if (ptr->GetID() == id) {
                return ptr;
            }
        }

        return nullptr;
    }

    Controller::SupportedProtocolCapability* Controller::FindSupportedProtocolCapability() const {
        static constexpr uint8_t SUPPORTED_PROTOCOL_ID = 2;
        return reinterpret_cast<SupportedProtocolCapability*>(FindExtendedCapability(SUPPORTED_PROTOCOL_ID));
    }

    Controller* Controller::Initialize(
        uint8_t bus,
        uint8_t device,
        uint8_t function,
        void* configuration_space
    ) {
        const int vector = Interrupts::ReserveInterrupt();

        // first check we can reserve an interrupt vector
        if (vector < 0) {
            return nullptr;
        }

        void* const raw_controller = Heap::Allocate(sizeof(Controller));

        if (raw_controller == nullptr) {
            return nullptr;
        }

        PCI::Interface basic_interface = PCI::Interface(bus, device, function, configuration_space);

        Controller* const controller = new(raw_controller) Controller(basic_interface);

        controller->interrupt_vector = vector;

        if (!controller->ConfigureMMIO()) {
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] MMIO initialized\n\r");

        // reset hub
        if (!controller->ResetHost()) {
            Log::putsSafe("[xHCI] Failed to reset host controller\n\r");
            Release(controller);
            return nullptr;
        }

        // configure the "max device slots enabled" field
        controller->ConfigureMaxSlotsEnabled();

        Log::printfSafe("[xHCI] Max slots: %llu\n\r", controller->max_slots_enabled);

        // program the DCBAAP
        if (!controller->ConfigureDCBAAP()) {
            Log::putsSafe("[xHCI] Failed to configure DCBAAP\n\r");
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] DCBAAP configured\n\r");

        // program the CRCR
        if (!controller->ConfigureCommandRing()) {
            Log::putsSafe("[xHCI] Failed to configure command ring\n\r");
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] Command ring configured\n\r");

        // program the event ring
        if (!controller->ConfigureEventRing()) {
            Log::putsSafe("[xHCI] Failed to configure event ring\n\r");
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] Event ring configured\n\r");

        // allocate scratchpad buffers
        if (!controller->ConfigureScratchpad()) {
            Log::putsSafe("[xHCI] Failed to configure scratchpad buffers\n\r");
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] Scratchpad buffers initialized\n\r");

        // Program the primary interrupter
        controller->ConfigurePrimaryInterrupter();

        // leave the interrupt IMODI to 4000 = 1ms
        
        // enable host interrupts
        controller->EnableHostInterrupts();

        // set interrupt enable and interrupt pending on the primary interrupter
        controller->EnablePrimaryInterrupter();

        // configure MSI for the controller
        const uint8_t LID = APIC::GetLAPICLogicalID();

        // // use current logical APIC as destination
        const uint32_t MA =
            (0x0FEE << 20)
            | (LID << 12)
            | (1 << 3)
            | (1 << 2);
        
        // // Use edge, lowest priority, 0x42 int vector
        const uint16_t MD =
            (0 << 15)
            | (1 << 8)
            | (static_cast<uint16_t>(vector));

        auto msi_cap = PCI::GetMSI(controller->interface);

        if (msi_cap == nullptr) {
            Release(controller);
            return nullptr;
        }

        msi_cap->ConfigureMSI(msi_cap, PCI::MSIConfiguration {
            .address = MA,
            .data = MD,
            .implemented_vectors = 1
        });

        msi_cap->Enable();

        // register interrupt handler
        Interrupts::RegisterIRQ(controller->interrupt_vector, controller);

        Log::putsSafe("[xHCI] Interrupts configured\n\r");

        // turn on the controller
        controller->EnableHost();

        controller->interface.EnableBusMaster();

        Log::putsSafe("[xHCI] Host online\n\r");

        if (!controller->ConfigurePortVersions()) {
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] Configured USB port versions\n\r");

        // wait 200 ms to let the controller initialize itself
        Self().SpinWaitMillis(200);

        const auto updaterTask = Scheduling::KernelTaskContext::Create(
            reinterpret_cast<void*>(&Controller::UpdatePortsTrampoline),
            reinterpret_cast<uint64_t>(controller)
        );

        if (!updaterTask.HasValue()) {
            Log::putsSafe("[xHCI] Failed to create port updater task\n\r");
            Release(controller);
            return nullptr;
        }

        controller->port_updater_task_id = Self().GetTaskManager().AddTask(updaterTask.GetValue());
        if (controller->port_updater_task_id == 0) {
            Log::putsSafe("[xHCI] Failed to schedule port updater task\n\r");
            Release(controller);
            return nullptr;
        }

        Log::putsSafe("[xHCI] Port updater task configured\n\r");

        return controller;
    }

    void Controller::Release(Controller* c) {
        c->Destroy();
        Heap::Free(c);
    }

    bool Controller::ResetHost() const {
        opregs->USBCMD |= USBCMD_HCRST_FLAG;

        auto& timer = Self().GetTimer();

        const uint64_t target = timer.GetCountMillis() + max_timeout_ms;

        // wait for controller to be ready
        while ((opregs->USBSTS & USBSTS_CNR_MASK) != 0) {
            if (timer.GetCountMillis() > target) {
                return false;
            }
            __asm__ volatile("pause");
        }

        return true;
    }

    void Controller::EnableHost() const {
        opregs->USBCMD |= USBCMD_RS_FLAG;
    }

    void Controller::DisableHost() const {
        opregs->USBCMD &= USBCMD_RS_FLAG;
    }

    void Controller::EnableHostInterrupts() const {
        opregs->USBCMD |= USBCMD_INTE_FLAG;
    }

    void Controller::DisableHostInterrupts() const {
        opregs->USBCMD &= ~USBCMD_INTE_FLAG;
    }

    bool Controller::GetCommandCycle() const {
        return command_cycle;
    }

    Optional<CommandCompletionEventTRB> Controller::SendCommand(const CommandTRB& trb) {
        static constexpr uint64_t COMPLETION_TIMEOUT_MS = 200;
        static constexpr auto COMMAND_STATUS_PREDICATE = [](void* arg) {
            const Controller* controller = reinterpret_cast<Controller*>(arg);
            return __blatomic_load_4(&controller->command_completion.data[2], Utils::MemoryOrder::SEQ_CST) != 0;
        };

        Utils::LockGuard _{command_lock};

        EnqueueCommand(trb);
        UpdateCommandPointer();

        command_completion.data[0] = 0;
        command_completion.data[1] = 0;
        command_completion.data[2] = 0;
        command_completion.data[3] = 0;

        SignalCommand();

        if (!Self().SpinWaitMillsFor(COMPLETION_TIMEOUT_MS, COMMAND_STATUS_PREDICATE, this)) {
            return Optional<CommandCompletionEventTRB>();
        }

        return Optional(command_completion);
    }

    size_t Controller::GetContextSize() const {
        if (context_size == 0) {
            static constexpr uint32_t HCCPARAMS1_CSZ_MASK = 0x00000004;
            static constexpr size_t DEFAULT_CONTEXT_SIZE = 32;
            static constexpr size_t EXTENDED_CONTEXT_SIZE = 64;

            context_size =
                (capability->HCCPARAMS1 & HCCPARAMS1_CSZ_MASK) == 0 ?
                DEFAULT_CONTEXT_SIZE : EXTENDED_CONTEXT_SIZE;
        }

        return context_size;
    }

    void Controller::Destroy() {
        ReleaseResources();
    }
}
