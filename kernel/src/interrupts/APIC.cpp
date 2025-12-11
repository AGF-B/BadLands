#include <cpuid.h>
#include <cstdint>

#include <new>

#include <shared/memory/defs.hpp>

#include <acpi/Interface.hpp>
#include <acpi/tables.hpp>

#include <ext/BasicHashMap.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/InterruptProvider.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/PIC.hpp>

#include <mm/VirtualMemory.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace {
	struct LAPIC_Interface {
	private:
		alignas(0x10) volatile			uint32_t Reserved_0[8];
		alignas(0x10) volatile mutable	uint32_t ID_REG;
		alignas(0x10) volatile			uint32_t VER_REG;
		alignas(0x10) volatile			uint32_t Reserved_1[16];
		alignas(0x10) volatile mutable	uint32_t TPR;
		alignas(0x10) volatile			uint32_t APR;
		alignas(0x10) volatile			uint32_t PPR;
		alignas(0x10) volatile mutable	uint32_t EOI_REG;
		alignas(0x10) volatile			uint32_t RRD;
		alignas(0x10) volatile mutable	uint32_t LDR;
		alignas(0x10) volatile mutable	uint32_t DFR;
		alignas(0x10) volatile mutable	uint32_t SVR;
		alignas(0x10) volatile			struct { alignas(0x10) volatile uint32_t data; } ISR[8];
		alignas(0x10) volatile			struct { alignas(0x10) volatile uint32_t data; } TMR[8];
		alignas(0x10) volatile			struct { alignas(0x10) volatile uint32_t data; } IRR[8];
		alignas(0x10) volatile mutable	uint32_t ESR;
		alignas(0x10) volatile			uint32_t Reserved_2[24];
		alignas(0x10) volatile mutable  uint32_t LVT_CMCI;
		alignas(0x10) volatile mutable	struct { alignas(0x10) volatile mutable uint32_t data; } ICR[2];
		alignas(0x10) volatile mutable	uint32_t LVT_TIMER;
		alignas(0x10) volatile mutable	uint32_t LVT_THERMAL_SENSOR;
		alignas(0x10) volatile mutable	uint32_t LVT_PMC;
		alignas(0x10) volatile mutable	uint32_t LVT_LINT0;
		alignas(0x10) volatile mutable	uint32_t LVT_LINT1;
		alignas(0x10) volatile mutable	uint32_t LVT_ERROR;
		alignas(0x10) volatile mutable	uint32_t TIMER_INITIAL_COUNT;
		alignas(0x10) volatile			uint32_t TIMER_CURRENT_COUNT;
		alignas(0x10) volatile			uint32_t Reserved_3[16];
		alignas(0x10) volatile mutable	uint32_t TIMER_DIVIDE_CONFIGURATION;
		alignas(0x10) volatile			uint32_t Reserved_4;

	public:
		static void SpuriousHandler(void*,uint64_t) {}

		uint8_t GetID() const {
			return (ID_REG >> 24) & 0xFF;
		}

		uint8_t GetVersion() const {
			return VER_REG & 0xFF;
		}

		void SendEOI() {
			EOI_REG = 0;
		}

		void SetLogicalID(uint8_t id) {
			LDR = (uint32_t)id << 24;
		}

		uint8_t GetLogicalID() const {
			return (LDR >> 24) & 0xFF;
			return 0;
		}

		void SetSVR(uint32_t svr) {
			SVR = svr;
		}

		void ResetESR() {
			ESR = 0;
		}

		void SetTimerLVT(uint8_t vector, APIC::Timer::Mode mode) {
			const uint32_t timer_mode =
				mode == APIC::Timer::Mode::ONE_SHOT ? 0 :
				mode == APIC::Timer::Mode::PERIODIC ? 1 :
				mode == APIC::Timer::Mode::TSC_DEADLINE ? 2
				: 0;

			LVT_TIMER = (timer_mode << 17) | 0x00010000 | static_cast<uint32_t>(vector);
		}

		void SetTimerDivideConfiguration(APIC::Timer::DivideConfiguration config) {
			const uint32_t divide_value =
				config == APIC::Timer::DivideConfiguration::BY_1 ? 0b1011 :
				config == APIC::Timer::DivideConfiguration::BY_2 ? 0b0000 :
				config == APIC::Timer::DivideConfiguration::BY_4 ? 0b0001 :
				config == APIC::Timer::DivideConfiguration::BY_8 ? 0b0010 :
				config == APIC::Timer::DivideConfiguration::BY_16 ? 0b0011 :
				config == APIC::Timer::DivideConfiguration::BY_32 ? 0b1000 :
				config == APIC::Timer::DivideConfiguration::BY_64 ? 0b1001 :
				config == APIC::Timer::DivideConfiguration::BY_128 ? 0b1010
				: 0b1011;

			TIMER_DIVIDE_CONFIGURATION = divide_value;
		}

		void SetTimerInitialCount(uint32_t count) {
			TIMER_INITIAL_COUNT = count;
		}

		uint32_t GetTimerCurrentCount() {
			return TIMER_CURRENT_COUNT;
		}

		void UnmaskTimerLVT() {
			LVT_TIMER &= ~0x00010000;
		}

		void MaskTimerLVT() {
			LVT_TIMER |= 0x00010000;
		}
	};

	struct IOAPIC_Interface {
	private:
		static constexpr uint32_t IOAPICID = 0;
		static constexpr uint32_t IOAPICVER = 1;
		static constexpr uint32_t IOAPICARB = 2;
		static constexpr uint32_t IOREDTBL = 0x10;

		static constexpr uint64_t RTE_MASK = 0x10000;

		alignas(0x10) volatile mutable uint32_t IOREGSEL;
		alignas(0x10) volatile mutable uint32_t IOWIN;

		constexpr uint64_t ConvertDeliveryMode(APIC::IRQDescriptor& descriptor) {
			static constexpr uint64_t FIXED				= 0x000;
			static constexpr uint64_t LOWEST_PRIORITY	= 0x100;
			static constexpr uint64_t SMI				= 0x200;
			static constexpr uint64_t NMI				= 0x400;
			static constexpr uint64_t INIT				= 0x500;
			static constexpr uint64_t EXT_INIT			= 0x700;

			switch (descriptor.Delivery) {
			case APIC::IRQDeliveryMode::FIXED:
				return FIXED;
			case APIC::IRQDeliveryMode::LOWEST_PRIORITY:
				return LOWEST_PRIORITY;
			case APIC::IRQDeliveryMode::SMI:
				descriptor.InterruptVector = 0;
				descriptor.Trigger = APIC::IRQTrigger::EDGE;
				return SMI;
			case APIC::IRQDeliveryMode::NMI:
				descriptor.Trigger = APIC::IRQTrigger::EDGE;
				return NMI;
			case APIC::IRQDeliveryMode::INIT:
				descriptor.Trigger = APIC::IRQTrigger::EDGE;
				return INIT;
			case APIC::IRQDeliveryMode::EXT_INIT:
				descriptor.Trigger = APIC::IRQTrigger::EDGE;
				return EXT_INIT;
			default:
				return FIXED;
			}
		}

		constexpr uint64_t ConvertDestinationMode(const APIC::IRQDestinationMode& mode) {
			static constexpr uint64_t PHYSICAL = 0x000;
			static constexpr uint64_t LOGICAL = 0x800;

			switch (mode) {
			case APIC::IRQDestinationMode::Physical:
				return PHYSICAL;
			case APIC::IRQDestinationMode::Logical:
				return LOGICAL;
			default:
				return PHYSICAL;
			}
		}

		constexpr uint64_t ConvertPolarity(const APIC::IRQPolarity& polarity, uint8_t index) {
			static constexpr uint64_t HIGH	= 0x0000;
			static constexpr uint64_t LOW	= 0x2000;

			switch (polarity) {
			case APIC::IRQPolarity::ACTIVE_HIGH:
				return HIGH;
			case APIC::IRQPolarity::ACTIVE_LOW:
				return LOW;
			case APIC::IRQPolarity::RESERVED:
				return GetRedirectionEntry(index) & LOW;
			default:
				return HIGH;
			}
		}

		constexpr uint64_t ConvertTrigger(const APIC::IRQTrigger& trigger, uint8_t index) {
			static constexpr uint64_t EDGE	= 0x0000;
			static constexpr uint64_t LEVEL = 0x8000;

			switch (trigger) {
			case APIC::IRQTrigger::EDGE:
				return EDGE;
			case APIC::IRQTrigger::LEVEL:
				return LEVEL;
			case APIC::IRQTrigger::RESERVED:
				return GetRedirectionEntry(index) & LEVEL;
			default:
				return EDGE;
			}
		}

		constexpr uint64_t ConvertDestination(uint8_t destination, const APIC::IRQDestinationMode& mode) {
			static constexpr uint64_t SHIFT = 56;
			static constexpr uint64_t PHYSICAL_MASK = 0x0F;

			switch (mode) {
			case APIC::IRQDestinationMode::Physical:
				destination &= PHYSICAL_MASK;
				break;
			default:
				break;
			}

			return (uint64_t)destination << SHIFT;
		}

	public:
		void SetID(uint8_t ID) {
			IOREGSEL = IOAPICID;
			IOWIN = (uint32_t)(ID & 0xF) << 24;
		}

		uint8_t GetID() const {
			IOREGSEL = IOAPICID;
			return (IOWIN >> 24) & 0xF;
		}

		uint8_t GetVersion() const {
			IOREGSEL = IOAPICVER;
			return IOWIN & 0xFF;
		}

		uint16_t GetRedirectionsCount() const {
			IOREGSEL = IOAPICVER;
			return (uint16_t)((IOWIN >> 16) & 0xFF) + 1;
		}

		uint8_t GetArbitrationID() const {
			IOREGSEL = IOAPICARB;
			return (IOWIN >> 24) & 0xF;
		}

		uint64_t GetRedirectionEntry(uint8_t index) const {
			uint64_t entry = 0;

			if (index >= GetRedirectionsCount()) {
				return entry;
			}

			IOREGSEL = IOREDTBL + index * 2;
			entry = IOWIN;
			IOREGSEL = IOREDTBL + index * 2 + 1;
			entry |= ((uint64_t)IOWIN << 32);

			return entry;
		}

		void SetRedirectionEntry(uint8_t index, uint64_t value) {
			if (index < GetRedirectionsCount()) {
				IOREGSEL = IOREDTBL + index * 2;
				IOWIN = value & 0xFFFFFFFF;
				IOREGSEL = IOREDTBL + index * 2 + 1;
				IOWIN = (value >> 32) & 0xFFFFFFFF;
			}
		}

		void MaskRedirectionEntry(uint8_t index) {
			if (index < GetRedirectionsCount()) {
				IOREGSEL = IOREDTBL + index * 2;
				IOWIN |= RTE_MASK;
			}
		}

		void UnmaskRedirectionEntry(uint8_t index) {
			if (index < GetRedirectionsCount()) {
				IOREGSEL = IOREDTBL + index * 2;
				IOWIN &= ~RTE_MASK;
			}
		}

		void SetupRedirectionEntry(uint8_t index, APIC::IRQDescriptor descriptor) {
			if (index < GetRedirectionsCount()) {
				uint64_t rte = 0;

				rte |= ConvertDeliveryMode(descriptor);
				rte |= descriptor.InterruptVector;
				rte |= ConvertDestinationMode(descriptor.DestinationMode);
				rte |= ConvertPolarity(descriptor.Polarity, index);
				rte |= ConvertTrigger(descriptor.Trigger, index);
				rte |= descriptor.Masked ? RTE_MASK : 0;
				rte |= ConvertDestination(descriptor.Destination, descriptor.DestinationMode);

				SetRedirectionEntry(index, rte);
			}
		}
	};

	struct LAPIC {
		enum class _Status {
			ONLINE,
			CAPABLE,
			DISABLED
		} Status;

		uint32_t Flags;
		uint32_t ID;
		uint8_t UID;
	};

	struct IOAPIC {
		IOAPIC_Interface* VirtualAddress;
		uint32_t PhysicalAddress;
		uint32_t ID;		
		uint32_t GlobalSystemInterruptBase;
	};

	struct IntOverride {
		enum class _Polarity {
			CONFORM,
			HIGH,
			RESERVED,
			LOW
		};

		enum class _Trigger {
			CONFORM,
			EDGE,
			RESERVED,
			LEVEL
		};

		_Polarity Polarity;
		_Trigger Trigger;
		uint32_t GlobalSystemInterrupt;
	};

	static constexpr uint32_t PCAT_COMPAT = 0x00000001;

	static constexpr uint32_t APIC_ONLINE = 0x00000001;
	static constexpr uint32_t APIC_CAPABLE = 0x00000002;

	static void* PhysicalLocalAPIC = nullptr;
	static LAPIC_Interface* LocalAPIC = nullptr;

	static Ext::BasicHashMap<uint32_t, LAPIC, 64> LAPICs;
	static Ext::BasicHashMap<uint32_t, IOAPIC, 16, 1> IOAPICs;

	static Interrupts::InterruptTrampoline PIC_SpuriousTrampoline(
		reinterpret_cast<void(*)(void*,uint64_t)>(&PIC::SpuriousPIC)
	);
	static Interrupts::InterruptTrampoline LocalAPICSpuriousTrampoline(LocalAPIC->SpuriousHandler);

	static IntOverride InterruptOverrides[0x100];

	uint8_t ReserveLogicalID() {
		static uint8_t logicalID = 0x01;
		uint8_t current = logicalID;
		logicalID <<= 1;
		return current;
	}
}

namespace APIC {
	namespace Timer {
		void SetTimerLVT(uint8_t vector, Mode mode) {
			LocalAPIC->SetTimerLVT(vector, mode);
		}

		void SetTimerDivideConfiguration(DivideConfiguration config) {
			LocalAPIC->SetTimerDivideConfiguration(config);
		}

		void SetTimerInitialCount(uint32_t count) {
			LocalAPIC->SetTimerInitialCount(count);
		}

		uint32_t GetTimerCurrentCount() {
			return LocalAPIC->GetTimerCurrentCount();
		}

		void UnmaskTimerLVT() {
			LocalAPIC->UnmaskTimerLVT();
		}

		void MaskTimerLVT() {
			LocalAPIC->MaskTimerLVT();
		}
	}
	
	void Initialize() {
		Log::puts("[APIC] Initializing APIC platform...\n\r");

		for (uint32_t i = 0; i < 0x100; ++i) {
			InterruptOverrides[i] = IntOverride { .Polarity = IntOverride::_Polarity::CONFORM, .Trigger = IntOverride::_Trigger::CONFORM, .GlobalSystemInterrupt = i } ;
		}
		
		auto* physical_madt = ACPI::FindTable("APIC");

		if (physical_madt == nullptr) {
			Panic::PanicShutdown("APIC (MADT/APIC TABLE COULD NOT BE FOUND)\n\r");
		}
		else {
			Log::printf("[APIC] MADT found at physical address %#.16llx\n\r", physical_madt);
		}

		using MADT = ACPI::MADT;

		auto* madt = static_cast<MADT*>(ACPI::MapTable(physical_madt));

		if (madt == nullptr) {
			Panic::PanicShutdown("APIC (MADT/APIC TABLE COULD NOT BE MAPPED)\n\r");
		}
		else {
			Log::printf("[APIC] MADT mapped at virtual address %#.16llx\n\r", madt);
		}

		PhysicalLocalAPIC = reinterpret_cast<void*>(madt->LocalInterruptControlAddress);
		
		if (madt->Flags & PCAT_COMPAT) {
			Log::puts("[APIC] Dual-8259A setup detected. Initializing it to disable it...\n\r");
			
			for (unsigned int i = 0; i < 8; ++i) {
				Interrupts::RegisterIRQ(PIC::MasterPIC_IRQ_Remap + i, &PIC_SpuriousTrampoline);
				Interrupts::RegisterIRQ(PIC::SlavePIC_IRQ_Remap + i, &PIC_SpuriousTrampoline);
			}

			PIC::InitializePIC();

			Log::puts("[APIC] Dual-8259A setup disabled\n\r");
		}

		uint8_t* type = reinterpret_cast<uint8_t*>(madt + 1);

		size_t lapic_count = 0;

		while (type < reinterpret_cast<uint8_t*>(madt) + madt->hdr.Length) {
			switch (*type) {
				case MADT::LocalAPIC::_Type: {
					auto* ptr = reinterpret_cast<MADT::LocalAPIC*>(type);
					++lapic_count;
					type += ptr->Length;
					break;
				}
				default:
					type += *(type + 1);
					break;
			}
		}

		if (UnattachedSelf::AllocateProcessors(lapic_count) == nullptr) {
			Panic::PanicShutdown("COULD NOT ALLOCATE MEMORY FOR PROCESSORS DATA\n\r");
		}

		Log::printf("[APIC] Enumerated %llu local APICs\n\r", lapic_count);

		type = reinterpret_cast<uint8_t*>(madt + 1);

		while (type < reinterpret_cast<uint8_t*>(madt) + madt->hdr.Length) {
			switch (*type) {
				case MADT::LocalAPIC::_Type: {
					auto* ptr = reinterpret_cast<MADT::LocalAPIC*>(type);

					Log::printf(
						"[APIC] Local APIC: ID 0x%.2hhx, UID 0x%.2hhx, %s\n\r",
						ptr->APIC_ID,
						ptr->ACPI_Processor_UID,
						(ptr->Flags & APIC_ONLINE) != 0 ?
							"Online" : (ptr->Flags & APIC_CAPABLE) != 0 ?
								"Online capable" : "Disabled"
					);

					const bool isOnline = ptr->Flags & APIC_ONLINE;
					const bool isOnlineCapable = ptr->Flags & APIC_CAPABLE;

					LAPIC lapic = {
						.Status = isOnline != 0 ?
							LAPIC::_Status::ONLINE : isOnlineCapable ?
								LAPIC::_Status::CAPABLE : LAPIC::_Status::DISABLED,
						.Flags = ptr->Flags,
						.ID = ptr->APIC_ID,
						.UID = ptr->ACPI_Processor_UID
					};

					if (LAPICs.insert(lapic.ID, lapic) == nullptr) {
						Panic::PanicShutdown("APIC (COULD NOT CREATE ADEQUATE LOCAL APIC STRUCTURE)\n\r");
					}

					// initialize remote self structure
					new (&UnattachedSelf::AllocateRemote()) UnattachedSelf(
						lapic.ID,
						lapic.UID,
						isOnline,
						isOnlineCapable
					);

					type += ptr->Length;
					break;
				}
				case MADT::IOAPIC::_Type: {
					auto* ptr = reinterpret_cast<MADT::IOAPIC*>(type);

					Log::printf(
						"[APIC] I/O APIC: ID 0x%.2hhx, Address 0x%.8x, GSI 0x%.8x\n\r",
						ptr->IOAPIC_ID,
						ptr->IOAPIC_Address,
						ptr->GlobalSystemInterruptBase
					);

					constexpr uint32_t memory_flags = Shared::Memory::PTE_READWRITE | Shared::Memory::PTE_UNCACHEABLE;

					IOAPIC_Interface* vaddr = static_cast<IOAPIC_Interface*>(
						VirtualMemory::MapGeneralPages(reinterpret_cast<void*>(ptr->IOAPIC_Address), 1, memory_flags)
					);

					if (vaddr == nullptr) {
						Panic::PanicShutdown("APIC (COULD NOT MAP I/O APIC INTO VIRTUAL MEMORY)\n\r");
					}

					Log::puts("[APIC] Configuring hardware I/O APIC ID...\n\r");

					vaddr->SetID(ptr->IOAPIC_ID);

					Log::printf("[APIC] I/O APIC ID (0x%.2hhx) configured \n\r", ptr->IOAPIC_ID);

					IOAPIC ioapic = {
						.VirtualAddress = vaddr,
						.PhysicalAddress = ptr->IOAPIC_Address,
						.ID = ptr->IOAPIC_ID,
						.GlobalSystemInterruptBase = ptr->GlobalSystemInterruptBase
					};

					if (IOAPICs.insert(ioapic.ID, ioapic) == nullptr) {
						Panic::PanicShutdown("APIC (COULD NOT CREATE ADEQUATE LOCAL APIC STRUCTURE)\n\r");
					}

					type += ptr->Length;
					break;
				}
				case MADT::InterruptSourceOverride::_Type: {
					auto* ptr = reinterpret_cast<MADT::InterruptSourceOverride*>(type);

					Log::printf(
						"[APIC] Interrupt Source Override (bus 0x%.2hhx): 0x%.2hhx -> 0x%.8x %s %s\n\r",
						ptr->Bus,
						ptr->Source,
						ptr->GlobalSystemInterrupt,
						(ptr->Flags & 3) == 0 ? "CONFORM" : (ptr->Flags & 3) == 1 ? "HIGH" : (ptr->Flags & 3) == 2 ? "RESERVED" : "LOW",
						(ptr->Flags & 12) == 0 ? "CONFORM" : (ptr->Flags & 12) == 4 ? "EDGE" : (ptr->Flags & 12) == 8 ? "RESERVED" : "LEVEL"
					);

					IntOverride source_override = {
						.Polarity = (ptr->Flags & 3) == 0 ?
							IntOverride::_Polarity::CONFORM : (ptr->Flags & 3) == 1 ?
								IntOverride::_Polarity::HIGH : (ptr->Flags & 3) == 2 ?
									IntOverride::_Polarity::RESERVED : IntOverride::_Polarity::LOW,
						.Trigger = (ptr->Flags & 12) == 0 ?
							IntOverride::_Trigger::CONFORM : (ptr->Flags & 12) == 4 ?
								IntOverride::_Trigger::EDGE : (ptr->Flags & 12) == 8 ?
									IntOverride::_Trigger::RESERVED : IntOverride::_Trigger::LEVEL,
						.GlobalSystemInterrupt = ptr->GlobalSystemInterrupt
					};

					InterruptOverrides[ptr->Source] = source_override;

					type += ptr->Length;
					break;
				}
				case MADT::APICOverride::_Type: {
					auto* ptr = reinterpret_cast<MADT::APICOverride*>(type);

					PhysicalLocalAPIC = reinterpret_cast<void*>(ptr);

					Log::printf("[APIC] Relocating Local APIC to 0x%.16llx\n\r", PhysicalLocalAPIC);

					type += ptr->Length;
					break;
				}
				default:
					type += *(type + 1);
					break;
			}
		}

		if (!ACPI::UnmapTable(madt)) {
			Panic::PanicShutdown("APIC (COULD NOT UNMAP MADT)\n\r");
		}

		LocalAPIC = reinterpret_cast<LAPIC_Interface*>(VirtualMemory::MapGeneralPages(
			PhysicalLocalAPIC, 1, Shared::Memory::PTE_READWRITE | Shared::Memory::PTE_UNCACHEABLE
		));

		if (LocalAPIC == nullptr) {
			Panic::PanicShutdown("APIC (COULD NOT MAP LOCAL xAPIC INTO VIRTUAL MEMORY)\n\r");
		}

		Log::printf("[APIC] Mapped LAPICs (0x%.16llx) to 0x%.16llx\n\r", PhysicalLocalAPIC, LocalAPIC);
		Log::puts("[APIC] Initialization done\n\r");
	}

	uint64_t rdmsr(uint32_t msr) {
		uint32_t low, high;
		__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
		return ((uint64_t)high << 32) | low;
	}

	void wrmsr(uint32_t msr, uint64_t value) {
		uint32_t low = value;
		uint32_t high = value >> 32;
		__asm__ volatile("wrmsr" :: "a"(low), "d"(high), "c"(msr));
	}

	void SetupLocalAPIC() {
		{
			static constexpr uint32_t APIC_FLAG = 0x00000200;

			unsigned int ebx, ecx, edx, unused;
			__cpuid(1, unused, ebx, ecx, edx);

			if ((edx & APIC_FLAG) == 0) {
				Panic::PanicShutdown("APIC (A PROCESSOR DOES NOT SUPPORT APIC)\n\r");
			}
		}

		static constexpr uint64_t xAPIC_GLOBAL_ENABLE = 0x00000800;

		uint64_t apic_base = rdmsr(0x01B);

		if ((apic_base & xAPIC_GLOBAL_ENABLE) == 0) {
			Panic::PanicShutdown("APIC (xAPIC GLOBALLY DISABLED)\n\r");
		}

		Log::printf("[APIC] Configuring LAPIC with ID 0x%.2hhx\n\r", LocalAPIC->GetID());

		static constexpr uint32_t SVR_CONFIG = 0x000001FF;
		static constexpr uint8_t SPURIOUS_IRQ_VECTOR = 0xFF;

		Interrupts::RegisterIRQ(SPURIOUS_IRQ_VECTOR, &LocalAPICSpuriousTrampoline);

		LocalAPIC->SetSVR(SVR_CONFIG);
		LocalAPIC->ResetESR();
		LocalAPIC->SendEOI();
		LocalAPIC->SetLogicalID(ReserveLogicalID());

		uint8_t logicalID = LocalAPIC->GetLogicalID();

		if (logicalID != 0) {
			Log::printf("[APIC] LAPIC 0x%.2hhx capable of receiving IRQ, logical ID 0x%.2hhx\n\r", LocalAPIC->GetID(), LocalAPIC->GetLogicalID());
		}

		Log::printf("[APIC] LAPIC 0x%.2hhx configured\n\r", LocalAPIC->GetID());
	}

	uint8_t GetLAPICLogicalID() {
		return LocalAPIC->GetLogicalID();
	}

	uint8_t GetLAPICID() {
		return LocalAPIC->GetID();
	}

	void SendEOI() {
		LocalAPIC->SendEOI();
	}

	void MaskIRQ(uint32_t irq) {
		if (irq < 0x100) {
			irq = InterruptOverrides[irq].GlobalSystemInterrupt;
		}

		for (auto* ioapic : IOAPICs) {
			uint32_t base = ioapic->GlobalSystemInterruptBase;
			uint32_t rteCount = ioapic->VirtualAddress->GetRedirectionsCount();

			if (base <= irq && irq < base + rteCount) {
				uint8_t pin = static_cast<uint8_t>(irq - base);

				ioapic->VirtualAddress->MaskRedirectionEntry(pin);
			}
		}
	}

	void UnmaskIRQ(uint32_t irq) {
		if (irq < 0x100) {
			irq = InterruptOverrides[irq].GlobalSystemInterrupt;
		}

		for (auto* ioapic : IOAPICs) {
			uint32_t base = ioapic->GlobalSystemInterruptBase;
			uint32_t rteCount = ioapic->VirtualAddress->GetRedirectionsCount();

			if (base <= irq && irq < base + rteCount) {
				uint8_t pin = static_cast<uint8_t>(irq - base);

				ioapic->VirtualAddress->UnmaskRedirectionEntry(pin);
			}
		}
	}

	void SetupIRQ(uint32_t irq, IRQDescriptor descriptor) {
		if (irq < 0x100) {
			auto remap = InterruptOverrides[irq];

			irq = remap.GlobalSystemInterrupt;
			
			switch (remap.Polarity) {
			case IntOverride::_Polarity::CONFORM:
				break;
			case IntOverride::_Polarity::HIGH:
				descriptor.Polarity = IRQPolarity::ACTIVE_HIGH;
				break;
			case IntOverride::_Polarity::LOW:
				descriptor.Polarity = IRQPolarity::ACTIVE_LOW;
				break;
			case IntOverride::_Polarity::RESERVED:
				descriptor.Polarity = IRQPolarity::RESERVED;
				break;
			}

			switch (remap.Trigger) {
			case IntOverride::_Trigger::CONFORM:
				break;
			case IntOverride::_Trigger::EDGE:
				descriptor.Trigger = IRQTrigger::EDGE;
				break;
			case IntOverride::_Trigger::LEVEL:
				descriptor.Trigger = IRQTrigger::LEVEL;
				break;
			case IntOverride::_Trigger::RESERVED:
				descriptor.Trigger = IRQTrigger::RESERVED;
				break;
			}
		}

		for (auto* ioapic : IOAPICs) {
			uint32_t base = ioapic->GlobalSystemInterruptBase;
			uint32_t rteCount = ioapic->VirtualAddress->GetRedirectionsCount();

			if (base <= irq && irq < base + rteCount) {
				uint8_t pin = static_cast<uint8_t>(irq - base);

				ioapic->VirtualAddress->SetupRedirectionEntry(pin, descriptor);
			}
		}
	}
}
