#pragma once

#include <cstddef>
#include <cstdint>

namespace PhysicalMemory {
	uint64_t FilterAddress(uint64_t address);
	uint64_t FilterAddress(void* address);

	enum class StatusCode {
		SUCCESS,
		OUT_OF_MEMORY,
		INVALID_PARAMETER,
		FREE,
		ALLOCATED
	};

	StatusCode Setup();

	uint64_t QueryMemoryUsage();
	StatusCode QueryDMAAddress(uint64_t address);

	void* AllocateDMA(uint64_t pages);
	void* Allocate();
	void* AllocatePages(uint64_t pages, uint64_t alignment = 0);

	StatusCode FreeDMA(void* ptr, uint64_t pages);
	StatusCode Free(void* ptr);
	StatusCode FreePages(void* ptr, uint64_t pages);

	void* Allocate2MBPage();
	StatusCode Free2MBPage(void* ptr);

	void* Allocate1GBPage();
	StatusCode Free1GBPage(void* ptr);
}
