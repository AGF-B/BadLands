#pragma once

namespace Interrupts {
	void kernel_idt_setup(void);
	//void register_irq(unsigned int irqLine, void(*handler)(void), unsigned int isTrap);
}
