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
