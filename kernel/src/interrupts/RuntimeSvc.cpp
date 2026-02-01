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

#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

namespace Runtime {
	static const EFI_RUNTIME_SERVICES* rtServices = nullptr;
	
	void Initialize() {
		rtServices = *reinterpret_cast<EFI_RUNTIME_SERVICES**>(
			Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.RTServices
		);
	}

	const EFI_RUNTIME_SERVICES* GetServices() {
		return rtServices;
	}
}
