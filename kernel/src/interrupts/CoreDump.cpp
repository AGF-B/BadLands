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

#include <cstddef>
#include <cstdint>

#include <interrupts/CoreDump.hpp>
#include <mm/VirtualMemoryLayout.hpp>
#include <screen/Log.hpp>

namespace {
	struct PanicStackView {
		uint64_t R15;
		uint64_t R14;
		uint64_t R13;
		uint64_t R12;
		uint64_t R11;
		uint64_t R10;
		uint64_t R9;
		uint64_t R8;
		uint64_t RDI;
		uint64_t RSI;
		uint64_t RBP;
		uint64_t RBX;
		uint64_t RDX;
		uint64_t RCX;
		uint64_t RAX;
		uint64_t INTERRUPT_VECTOR;
		uint64_t ERROR_CODE;
		uint64_t RIP;
		uint64_t CS;
		uint64_t RFLAGS;
		uint64_t RSP;
		uint64_t SS;
	};

#pragma pack(push)
#pragma pack(1)
	struct DescriptorTableInfo {
		uint16_t LIMIT;
		uint64_t BASE;
	};
#pragma pack(pop)
}

void Panic::DumpCore(void* panic_stack, uint64_t errv) {
	Log::puts("\n\r------ CORE DUMP ------\n\r");

	PanicStackView* psv = reinterpret_cast<PanicStackView*>(panic_stack);

	const uint8_t CPL = psv->CS & 0x3;

	uint16_t DS;
	uint16_t ES;
	uint16_t FS;
	uint16_t GS;
	uint16_t LDTSS;
	uint16_t TRSS;
	DescriptorTableInfo GDT;
	DescriptorTableInfo IDT;
	uint64_t CR0;
	uint64_t CR2;
	uint64_t CR3;
	uint64_t CR4;
	uint64_t CR8;
	uint64_t EFER;
	uint64_t DR0;
	uint64_t DR1;
	uint64_t DR2;
	uint64_t DR3;
	uint64_t DR6;
	uint64_t DR7;

	__asm__ volatile("mov %%ds, %0" : "=r"(DS));
	__asm__ volatile("mov %%es, %0" : "=r"(ES));
	__asm__ volatile("mov %%fs, %0" : "=r"(FS));
	__asm__ volatile("mov %%gs, %0" : "=r"(GS));
	__asm__ volatile("sldt %0" : "=r"(LDTSS));
	__asm__ volatile("str %0" : "=r"(TRSS));
	__asm__ volatile("sgdt %0" : "=m"(GDT));
	__asm__ volatile("sidt %0" : "=m"(IDT));
	__asm__ volatile("mov %%cr0, %0" : "=r"(CR0));
	__asm__ volatile("mov %%cr2, %0" : "=r"(CR2));
	__asm__ volatile("mov %%cr3, %0" : "=r"(CR3));
	__asm__ volatile("mov %%cr4, %0" : "=r"(CR4));
	__asm__ volatile("mov %%cr8, %0" : "=r"(CR8));
	uint32_t l,h;
	static constexpr uint32_t EFER_MSR_NUMBER = 0xC0000080;
	__asm__ volatile("rdmsr" : "=a"(l), "=d"(h) : "c"(EFER_MSR_NUMBER));
	EFER = ((uint64_t)h << 32) | l;
	__asm__ volatile("mov %%dr0, %0" : "=r"(DR0));
    __asm__ volatile("mov %%dr1, %0" : "=r"(DR1));
    __asm__ volatile("mov %%dr2, %0" : "=r"(DR2));
    __asm__ volatile("mov %%dr3, %0" : "=r"(DR3));
    __asm__ volatile("mov %%dr6, %0" : "=r"(DR6));
    __asm__ volatile("mov %%dr7, %0" : "=r"(DR7));

	Log::printf(" RAX=%0.16llx RBX=%0.16llx RCX=%0.16llx RDX=%0.16llx\n\r", psv->RAX, psv->RBX, psv->RCX, psv->RDX);
	Log::printf(" RSI=%0.16llx RDI=%0.16llx RBP=%0.16llx RSP=%0.16llx\n\r", psv->RSI, psv->RDI, psv->RBP, psv->RSP);
	Log::printf(" R8 =%0.16llx R9 =%0.16llx R10=%0.16llx R11=%0.16llx\n\r", psv->R8, psv->R9, psv->R10, psv->R11);
	Log::printf(" R12=%0.16llx R13=%0.16llx R14=%0.16llx R15=%0.16llx\n\r", psv->R12, psv->R13, psv->R14, psv->R15);
	Log::printf(" RIP=%0.16llx RFL=%0.16llx CPL=%hhu E=0x%.016llx\n\r", psv->RIP, psv->RFLAGS, CPL, errv);
	Log::printf(" ES =%0.4hx\n\r", ES);
	Log::printf(" CS =%0.4hx\n\r", psv->CS);
	Log::printf(" SS =%0.4hx\n\r", psv->SS);
	Log::printf(" DS =%0.4hx\n\r", DS);
	Log::printf(" FS =%0.4hx\n\r", FS);
	Log::printf(" GS =%0.4hx\n\r", GS);
	Log::printf(" LDT=%0.4hx\n\r", LDTSS);
	Log::printf(" GDT=---- %0.16llx %0.8x\n\r", GDT.BASE, GDT.LIMIT);
	Log::printf(" IDT=---- %0.16llx %0.8x\n\r", IDT.BASE, IDT.LIMIT);
	Log::printf(" CR0=%0.16llx CR2=%0.16llx CR3=%0.16llx CR4=%0.16llx\n\r", CR0, CR2, CR3, CR4);
	Log::printf(" CR8=%0.16llx EFER=%0.16llx\n\r", CR8, EFER);
	Log::printf(" DR0=%0.16llx DR1=%0.16llx DR2=%0.16llx DR3=%0.16llx\n\r", DR0, DR1, DR2, DR3);
	Log::printf(" DR6=%0.16llx DR7=%0.16llx\n\r", DR6, DR7);
}
