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

#include <fs/Status.hpp>
#include <fs/IFNode.hpp>

namespace FS {
    IFNode::IFNode(Owner* owner) : owner(owner) {}

    FS::Status IFNode::Open() {
        if (ShouldBeRemoved()) {
            return FS::Status::UNAVAILABLE;
        }

        ++openReferences;

        return FS::Status::SUCCESS;
    }

    void IFNode::Close() {
        --openReferences;

        if (ShouldBeRemoved() && openReferences == 0) {
            Destroy();
        }
    }

    size_t IFNode::GetOpenReferences() {
        return openReferences;
    }

    void IFNode::MarkForRemoval() {
        removed = true;
    }

    bool IFNode::ShouldBeRemoved() {
        return removed;
    }

    Directory::Directory(Owner* owner) : IFNode(owner) {}

    Response<size_t> Directory::Read([[maybe_unused]] size_t offset, [[maybe_unused]] size_t count, [[maybe_unused]] uint8_t* buffer) {
        return Response<size_t>(Status::UNSUPPORTED);
    }

    Response<size_t> Directory::Write([[maybe_unused]] size_t offset, [[maybe_unused]] size_t count, [[maybe_unused]] const uint8_t* buffer) {
        return Response<size_t>(Status::UNSUPPORTED);
    }

    File::File(Owner* owner) : IFNode(owner) {}

    Response<IFNode*> File::Find([[maybe_unused]] const DirectoryEntry& fileref) {
        return Response<IFNode*>(Status::UNSUPPORTED);
    }

    Status File::Create([[maybe_unused]] const DirectoryEntry& fileref, [[maybe_unused]] FileType type) {
        return Status::UNSUPPORTED;
    }

    Status File::AddNode([[maybe_unused]] const DirectoryEntry& fileref, [[maybe_unused]] IFNode* node) {
        return Status::UNSUPPORTED;
    }

    Status File::Remove([[maybe_unused]] const DirectoryEntry& fileref) {
        return Status::UNSUPPORTED;
    }

    Response<size_t> File::List([[maybe_unused]] DirectoryEntry* list, [[maybe_unused]] size_t length, [[maybe_unused]] size_t from) {
        return Response<size_t>(Status::UNSUPPORTED);
    }
}
