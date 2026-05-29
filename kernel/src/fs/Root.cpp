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

#include <shared/memory/layout.hpp>

#include <devices/Block/Device.hpp>
#include <fs/Root.hpp>
#include <mm/Utils.hpp>

using GUID = Devices::Block::GUID;

namespace {
    static GUID rootPartitionGUID = { .data = { 0 } };
}

namespace FS {
    void InitializeRootPartitionDetection() {
        const GUID* guid = reinterpret_cast<const GUID*>(
            Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.RootUUID
        );

        Utils::memcpy(rootPartitionGUID.data, guid->data, sizeof(GUID));
    }

    bool IsRootPartition(const GUID& guid) {
        return Utils::memcmp(rootPartitionGUID.data, guid.data, sizeof(GUID)) == 0;
    }
}
