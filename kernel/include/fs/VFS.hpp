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

#include <fs/IFNode.hpp>
#include <fs/NPFS.hpp>
#include <fs/Status.hpp>

class VFS final : public NPFS {
private:
    VFS();

    static bool         CheckFilePath(const FS::DirectoryEntry& filepath);
    
    static bool         IsApplicationPath(const FS::DirectoryEntry& filepath);
    static FS::Status   HandleApplicationPath(
        const FS::DirectoryEntry& filepath,
        FS::DirectoryEntry& current,
        FS::IFNode*& node
    );

    static FS::Response<FS::DirectoryEntry> ExtractFileName(const FS::DirectoryEntry& filepath);

public:
    static bool Construct(VFS* fs);
    
    FS::Response<FS::IFNode*> OpenParent(
        const FS::DirectoryEntry& filepath,
        FS::DirectoryEntry& filename
    );
    FS::Response<FS::IFNode*> Open(const FS::DirectoryEntry& filepath);
};
