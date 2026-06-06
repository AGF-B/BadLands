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

#include <fs/FileSystem.hpp>
#include <fs/IFNode.hpp>
#include <fs/Bridge.hpp>

#include <kern/memory.hpp>

namespace FS {
    Response<kern::shared_ptr<IFNode>> Bridge::Find(const DirectoryEntry& fileref) {
        return fs->GetRoot()->Find(fileref);
    }

    Status Bridge::Create(const DirectoryEntry& fileref, FileType type) {
        return fs->GetRoot()->Create(fileref, type);
    }

    Status Bridge::AddNode(const DirectoryEntry& fileref, const kern::shared_ptr<IFNode>& node) {
        return fs->GetRoot()->AddNode(fileref, node);
    }

    Status Bridge::Remove(const DirectoryEntry& fileref) {
        return fs->GetRoot()->Remove(fileref);
    }

    Response<size_t> Bridge::List(DirectoryEntry* list, size_t length, size_t from) {
        return fs->GetRoot()->List(list, length, from);
    }

    Status Bridge::Query(const QueryInfo& info) {
        return fs->GetRoot()->Query(info);
    }

    void Bridge::Unregister() {
        fs->GetRoot()->Unregister();
    }
}
