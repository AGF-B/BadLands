#pragma once

#include <cstddef>

#include <shared/Lock.hpp>

#include <fs/IFNode.hpp>
#include <fs/Status.hpp>

class NPFS : public FS::Owner {
protected:
    class Directory final : public FS::Directory {
    public:
        explicit Directory(FS::Owner* owner);

        FS::Response<IFNode*>   Find(const FS::DirectoryEntry& fileref) final;
        FS::Status              Create(const FS::DirectoryEntry& fileref, FS::FileType type) final;
        FS::Status              AddNode(const FS::DirectoryEntry& fileref, FS::IFNode* node) final;
        FS::Status              Remove(const FS::DirectoryEntry& fileref) final;
        FS::Response<size_t>    List(FS::DirectoryEntry* list, size_t length, size_t from = 0) final;

        static bool             Construct(Directory* directory);
        void                    Destroy() final;

    private:
        struct DirectoryEntry;

        FS::Response<DirectoryEntry*>   FindEntry(const FS::DirectoryEntry& fileref);
        FS::Status                      CreateEntry(const DirectoryEntry* entry);

        void* container;
        Utils::Lock mut;
    };

    class File final : public FS::File {
    public:
        explicit File(FS::Owner* owner);

        FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
        FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

        static bool Construct(File* file);
        void        Destroy() final;
    
    private:
        void* container;
        Utils::Lock mut;
    };

    Directory root;

    NPFS();

public:
    static bool Construct(NPFS* fs);
};
