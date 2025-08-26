#pragma once

namespace Interrupts {
	void kernel_idt_setup(void);
	void RegisterIRQ(unsigned int interruptVector, void(*handler)(void), unsigned int isTrap);
}
