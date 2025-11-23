#pragma once

#include <cstddef>

#include <interrupts/InterruptProvider.hpp>

namespace Interrupts {
	void kernel_idt_setup(void);
	void ForceIRQHandler(unsigned int interruptVector, void* handler);
	void ReleaseIRQ(unsigned int interruptVector);
	void RegisterIRQ(unsigned int interruptVector, InterruptProvider* provider);
	int ReserveInterrupt();
	void ReleaseInterrupt(int i);
}
