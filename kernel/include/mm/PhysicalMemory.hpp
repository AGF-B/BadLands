#pragma once

#include <cstddef>
#include <cstdint>

#include <shared/Response.hpp>

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

	Success Setup();

	uint64_t QueryMemoryUsage();
	StatusCode QueryDMAAddress(uint64_t address);

	void* AllocateDMA(uint64_t pages);
	void* Allocate();
	void* Allocate2MB();
	void* Allocate32MB();

	Success FreeDMA(void* ptr, uint64_t pages);
	Success Free(void* ptr);
	Success Free2MB(void* ptr);
	Success Free32MB(void* ptr);
	Success Free1GB(void* ptr);
}
