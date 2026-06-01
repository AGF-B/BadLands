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

#include <shared/SimpleAtomic.hpp>

#include <fs/Status.hpp>

#include <kern/memory.hpp>

namespace FS {
    static inline constexpr size_t MAX_FILE_PATH = 4096;

    struct DirectoryEntry {
        size_t      NameLength;
        const char* Name;
    };

    struct QueryInfo {
        size_t queryId;
        size_t queryDataSize;
        size_t queryResultSize;
        void* queryData;
        void* queryResult;
    };

    enum class FileType {
        FILE,
        DIRECTORY
    };

    class Owner {};

    class IFNode {
    public:
        explicit IFNode(Owner* owner);

        virtual void    MarkForRemoval() final;
        virtual bool    ShouldBeRemoved() const final;
        
        virtual Status  CanBeOpened() const final;


        virtual Response<kern::shared_ptr<IFNode>> Find(const DirectoryEntry& fileref) = 0;
        virtual Status              Create(const DirectoryEntry& fileref, FileType type) = 0;
        virtual Status              AddNode(const DirectoryEntry& fileref, const kern::shared_ptr<IFNode>& node) = 0;
        virtual Status              Remove(const DirectoryEntry& fileref) = 0;
        virtual Response<size_t>    List(DirectoryEntry* list, size_t length, size_t from = 0) = 0;
        virtual bool                IsDirectory() const = 0;

        virtual Response<size_t>    Read(size_t offset, size_t count, uint8_t* buffer) = 0;
        virtual Response<size_t>    Write(size_t offset, size_t count, const uint8_t* buffer) = 0;

        virtual Status              Query(const QueryInfo& info) = 0;

        virtual void Unregister() = 0;
        
        ~IFNode() = default;

    protected:

        Owner* const owner;

    private:
        bool removed{false};
    };

    class Directory : public IFNode {
    public:
        explicit Directory(Owner* owner);

        virtual bool                IsDirectory() const final;

        virtual Response<size_t>    Read(size_t offset, size_t count, uint8_t* buffer) final;
        virtual Response<size_t>    Write(size_t offset, size_t count, const uint8_t* buffer) final;

        ~Directory() = default;
    };

    class File : public IFNode {
    public:
        explicit File(Owner* owner);

        virtual Response<kern::shared_ptr<IFNode>> Find(const DirectoryEntry& fileref) final;
        virtual Status			    Create(const DirectoryEntry& fileref, FileType type) final;
        virtual Status			    AddNode(const DirectoryEntry& fileref, const kern::shared_ptr<IFNode>& node) final;
        virtual Status			    Remove(const DirectoryEntry& fileref) final;
        virtual Response<size_t>	List(DirectoryEntry* list, size_t length, size_t from = 0) final;
        virtual bool                IsDirectory() const final;

        ~File() = default;
    };
}