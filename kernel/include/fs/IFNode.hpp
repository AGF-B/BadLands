#pragma once

#include <cstddef>
#include <cstdint>

#include <shared/SimpleAtomic.hpp>

#include <fs/Status.hpp>

namespace FS {
    static inline constexpr size_t MAX_FILE_PATH = 4096;

    struct DirectoryEntry {
        size_t      NameLength;
        const char* Name;
    };

    enum class FileType {
        FILE,
        DIRECTORY
    };

    class IFNode;

    class Owner {};

    class IFNode {
    public:
        explicit IFNode(Owner* owner);

        virtual FS::Status  Open() final;
        virtual void        Close() final;
        virtual size_t      GetOpenReferences() final;

        virtual void    MarkForRemoval() final;
        virtual bool    ShouldBeRemoved() final;


        virtual Response<IFNode*>   Find(const DirectoryEntry& fileref) = 0;
        virtual Status              Create(const DirectoryEntry& fileref, FileType type) = 0;
        virtual Status              AddNode(const DirectoryEntry& fileref, IFNode* node) = 0;
        virtual Status              Remove(const DirectoryEntry& fileref) = 0;
        virtual Response<size_t>    List(DirectoryEntry* list, size_t length, size_t from = 0) = 0;

        virtual Response<size_t>    Read(size_t offset, size_t count, uint8_t* buffer) = 0;
        virtual Response<size_t>    Write(size_t offset, size_t count, const uint8_t* buffer) = 0;

    protected:
        virtual void Destroy() = 0;

        Owner* const owner;

    private:
        Utils::SimpleAtomic<size_t> openReferences{0};
        bool removed{false};
    };

    class Directory : public IFNode {
    public:
        explicit Directory(Owner* owner);

        virtual Response<size_t>    Read(size_t offset, size_t count, uint8_t* buffer) final;
        virtual Response<size_t>    Write(size_t offset, size_t count, const uint8_t* buffer) final;
    };

    class File : public IFNode {
    public:
        explicit File(Owner* owner);

        virtual Response<IFNode*>   Find(const DirectoryEntry& fileref) final;
        virtual Status			    Create(const DirectoryEntry& fileref, FileType type) final;
        virtual Status			    AddNode(const DirectoryEntry& fileref, IFNode* node) final;
        virtual Status			    Remove(const DirectoryEntry& fileref) final;
        virtual Response<size_t>	List(DirectoryEntry* list, size_t length, size_t from = 0) final;
    };
}