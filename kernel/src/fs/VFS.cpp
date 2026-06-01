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

#include <new>

#include <fs/IFNode.hpp>
#include <fs/Status.hpp>
#include <fs/VFS.hpp>

VFS::VFS() : NPFS() {}

bool VFS::CheckFilePath(const FS::DirectoryEntry& filepath) {
    return !(
        filepath.Name == nullptr
        || filepath.NameLength > FS::MAX_FILE_PATH
        || filepath.NameLength < 2
        || filepath.Name[0] != '/'
    );
}

bool VFS::IsApplicationPath(const FS::DirectoryEntry& filepath) {
    return filepath.Name[1] != '/';
}

FS::Status VFS::HandleApplicationPath(
    const FS::DirectoryEntry& filepath,
    FS::DirectoryEntry& current,
    kern::shared_ptr<FS::IFNode>& node
) {
    const char applicationBase[] = "partitions";

    current.NameLength = 0;

    if (IsApplicationPath(filepath)) {
        auto result = node->Find({ .NameLength = sizeof(applicationBase) - 1, .Name = applicationBase });

        if (result.CheckError()) {
            return result.GetError();
        }

        node = result.GetValue();

        current.Name = filepath.Name + 1;
    }
    else {
        current.Name = filepath.Name + 2;
    }

    return {FS::Status::SUCCESS};
}

FS::Response<FS::DirectoryEntry> VFS::ExtractFileName(const FS::DirectoryEntry& filepath) {
    if (filepath.Name == nullptr || filepath.NameLength == 0 || filepath.NameLength > FS::MAX_FILE_PATH) {
        return {FS::Status::INVALID_PARAMETER};
    }

    FS::DirectoryEntry filename = { .NameLength = 0, .Name = filepath.Name + filepath.NameLength - 1 };

    if (filename.Name[0] == '/') {
        if (filepath.NameLength == 1) {
            return {filename};
        }

        --filename.Name;
    }

    while (filename.Name[0] != '/' && filename.Name > filepath.Name) {
        --filename.Name;
        ++filename.NameLength;
    }

    if (filename.Name != filepath.Name || filepath.Name[0] == '/') {
        ++filename.Name;
    }

    return {filename};
}

Success VFS::Construct(VFS* fs) {
    auto newfs = new(fs) VFS;

    newfs->root = kern::make_shared<Directory>(newfs);

    if (!newfs->root) {
        return Failure();
    }

    return NPFS::Directory::Construct(newfs->root.get());
}

FS::Response<kern::shared_ptr<FS::IFNode>> VFS::OpenParent(const FS::DirectoryEntry& filepath, FS::DirectoryEntry& filename) {
    if (!CheckFilePath(filepath)) {
        return {FS::Status::INVALID_PARAMETER};
    }

    auto extracted = ExtractFileName(filepath);

    if (extracted.CheckError()) {
        return {extracted.GetError()};
    }

    filename = extracted.GetValue();

    const size_t parentPathLength = (size_t)(filename.Name - filepath.Name);
    FS::DirectoryEntry parentpath = { .NameLength = parentPathLength, .Name = filepath.Name };

    auto status = root->CanBeOpened();

    if (status != FS::Status::SUCCESS) {
        return {status};
    }

    kern::shared_ptr<NPFS::Directory> local_root = root;

    kern::shared_ptr<FS::IFNode> node = kern::static_pointer_cast<FS::IFNode>(local_root);
    FS::DirectoryEntry current;

    auto response = HandleApplicationPath(parentpath, current, node);

    if (response != FS::Status::SUCCESS) {
        return {response};
    }

    for (size_t i = (size_t)(current.Name - parentpath.Name); i < parentpath.NameLength; ++i) {
        const char c = parentpath.Name[i];

        if (c == '/') {
            if (current.NameLength == 0) {
                return {FS::Status::INVALID_PARAMETER};
            }

            auto result = node->Find(current);
            
            if (result.CheckError()) {
                return {result.GetError()};
            }
            
            node = result.GetValue();

            current.Name += current.NameLength + 2;
            current.NameLength = 0;
        }
        else {
            ++current.NameLength;
        }
    }

    if (current.NameLength == 0) {
        return {node};
    }
    
    return {node->Find(current)};
}

FS::Response<kern::shared_ptr<FS::IFNode>> VFS::Open(const FS::DirectoryEntry& filepath) {
    FS::DirectoryEntry filename;

    auto result = OpenParent(filepath, filename);

    if (result.CheckError() || filename.NameLength == 0) {
        return result;
    }

    auto node = result.GetValue();

    result = node->Find(filename);

    return result;
}
