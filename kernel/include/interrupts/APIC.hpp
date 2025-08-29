#pragma once

namespace APIC {
	enum class IRQDeliveryMode {
		FIXED,
		LOWEST_PRIORITY,
		SMI,
		NMI,
		INIT,
		EXT_INIT
	};

	enum class IRQDestinationMode {
		Physical,
		Logical
	};

	enum class IRQPolarity {
		ACTIVE_HIGH,
		ACTIVE_LOW,
		RESERVED
	};

	enum class IRQTrigger {
		EDGE,
		LEVEL,
		RESERVED
	};

	struct IRQDescriptor {
		uint8_t InterruptVector;
		IRQDeliveryMode Delivery;
		IRQDestinationMode DestinationMode;
		IRQPolarity Polarity;
		IRQTrigger Trigger;
		bool Masked;
		uint8_t Destination;
	};

	void Initialize();

	void SetupLocalAPIC();
	uint8_t GetLAPICLogicalID();
	void SendEOI();

	void MaskIRQ(uint32_t irq);
	void SetupIRQ(uint32_t irq, IRQDescriptor descriptor);
}
