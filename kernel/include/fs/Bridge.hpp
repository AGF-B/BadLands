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

#include <kern/memory.hpp>

namespace FS {
    class Bridge : public Directory {
    private:
        const kern::unique_ptr<FileSystem> fs;

    public:
        inline Bridge(Owner* owner, kern::unique_ptr<FileSystem>&& fs) : Directory{owner}, fs{std::move(fs)} {}

        virtual Response<kern::shared_ptr<IFNode>> Find(const DirectoryEntry& fileref) final;
        virtual Status              Create(const DirectoryEntry& fileref, FileType type) final;
        virtual Status              AddNode(const DirectoryEntry& fileref, const kern::shared_ptr<IFNode>& node) final;
        virtual Status              Remove(const DirectoryEntry& fileref) final;
        virtual Response<size_t>    List(DirectoryEntry* list, size_t length, size_t from = 0) final;

        virtual Status Query(const QueryInfo& info) final;

        virtual void Unregister() final;

        ~Bridge() = default;
    };
}
