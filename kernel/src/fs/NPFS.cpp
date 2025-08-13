#include <cstddef>
#include <cctype>

#include <new>

#include <shared/LockGuard.hpp>

#include <fs/IFNode.hpp>
#include <fs/NPFS.hpp>
#include <fs/Status.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

namespace {
    class DataNode final {
        constexpr static size_t BLOCK_SIZE = 0x1000;
        constexpr static size_t JUNCTION_SIZE = 64;

        class DataStorageVector {
        public:
            explicit DataStorageVector(size_t depth) : depth(depth) {};

            size_t GetDepth() const {
                return depth;
            }

            virtual uint8_t* GetWeakBlock([[maybe_unused]] size_t blockId) {
                return nullptr;
            }

            virtual uint8_t* GetBlock() {
                return nullptr;
            }

            virtual FS::Response<uint8_t*> GetBlock([[maybe_unused]] size_t blockId) {
                return FS::Response<uint8_t*>(nullptr);
            }

            virtual void Destroy() { }

        protected:
            const size_t depth;
        };

        class TerminalDataVector final : public DataStorageVector {
        public:
            TerminalDataVector() : DataStorageVector(0) {};

            uint8_t* GetBlock() final {
                return data;
            }

        private:
            uint8_t data[BLOCK_SIZE] = { 0 };
        };

        class DataJunction final : public DataStorageVector {
        public:
            DataJunction(size_t depth) : DataStorageVector(depth) {}

            DataJunction(size_t depth, DataStorageVector* first) : DataStorageVector(depth) {
                junction[0] = first;
            }

            uint8_t* GetWeakBlock(size_t blockId) final {
                if (depth == 1) {
                    if (blockId >= JUNCTION_SIZE) {
                        return nullptr;
                    }

                    TerminalDataVector* tdv = (TerminalDataVector*)junction[blockId];

                    if (tdv == nullptr) {
                        return nullptr;
                    }

                    return tdv->GetBlock();
                }

                size_t reductor = 1;

                for (size_t r = 1; r < depth; ++r) {
                    reductor *= JUNCTION_SIZE;
                }

                size_t junctionId = blockId / reductor;
                size_t subId = blockId % reductor;

                if (junctionId >= JUNCTION_SIZE) {
                    return nullptr;
                }

                DataStorageVector* subVector = junction[junctionId];

                if (subVector == nullptr) {
                    return nullptr;
                }
                    
                return subVector->GetWeakBlock(subId);
            }

            FS::Response<uint8_t*> GetBlock(size_t blockId) final {
                if (depth == 1) {
                    if (blockId >= JUNCTION_SIZE) {
                        return FS::Response<uint8_t*>(FS::Status::OUT_OF_BOUNDS);
                    }

                    TerminalDataVector* tdv = (TerminalDataVector*)junction[blockId];

                    if (tdv == nullptr) {
                        void* mem = Heap::Allocate(sizeof(TerminalDataVector));
                        
                        if (mem == nullptr) {
                            return FS::Response<uint8_t*>(FS::Status::DEVICE_ERROR);
                        }

                        tdv = new(mem) TerminalDataVector();
                        junction[blockId] = tdv;
                    }

                    return FS::Response(tdv->GetBlock());
                }

                size_t reductor = 1;

                for (size_t r = 1; r < depth; ++r) {
                    reductor *= JUNCTION_SIZE;
                }

                size_t junctionId = blockId / reductor;
                size_t subId = blockId % reductor;

                if (junctionId >= JUNCTION_SIZE) {
                    return FS::Response<uint8_t*>(FS::Status::OUT_OF_BOUNDS);
                }

                DataStorageVector* subVector = junction[junctionId];

                if (subVector == nullptr) {
                    void* mem = Heap::Allocate(sizeof(DataJunction));

                    if (mem == nullptr) {
                        return FS::Response<uint8_t*>(FS::Status::DEVICE_ERROR);
                    }

                    subVector = new(mem) DataJunction(depth - 1);
                    junction[junctionId] = subVector;
                }

                return subVector->GetBlock(subId);
            }

            void Destroy() final {
                for (size_t i = 0; i < JUNCTION_SIZE; ++i) {
                    if (junction[i] != nullptr) {
                        junction[i]->Destroy();
                        Heap::Free(junction[i]);
                    }
                }
            }

        private:
            DataStorageVector* junction[JUNCTION_SIZE] = { nullptr };
        };

        DataStorageVector* data;

        DataNode() = default;

    public:
        static bool Construct(DataNode& result) {
            void* mem = Heap::Allocate(sizeof(DataJunction));

            if (mem == nullptr) {
                return false;
            }

            DataJunction* junction = new(mem) DataJunction(1);

            result.data = junction;

            return true;
        }

        size_t QueryBlockSize() const {
            return BLOCK_SIZE;
        }

        uint8_t* GetWeakBlock(size_t blockId) const {
            return data->GetWeakBlock(blockId);
        }

        uint8_t* GetBlock(size_t blockId) {
            size_t remaining_attempts = 5;

            auto status = data->GetBlock(blockId);            

            while (status.CheckError()) {
                if (--remaining_attempts == 0) {
                    return nullptr;
                }

                void* mem = Heap::Allocate(sizeof(DataJunction));

                if (mem == nullptr) {
                    return nullptr;
                }

                data = new(mem) DataJunction(data->GetDepth() + 1, data);
                status = data->GetBlock(blockId);
            }

            return status.GetValue();
        }

        void Destroy() {
            data->Destroy();
            Heap::Free(data);
        }
    };

    struct DirectoryData {
        DataNode data;
    };

    struct FileData {
        DataNode data;
        size_t size = 0;
    };

    static size_t GetEffectiveEnd(size_t offset, size_t count, size_t size) {
        if (offset + count > size || offset + count < offset) {
            return size;
        }

        return offset + count;
    }
}

struct NPFS::Directory::DirectoryEntry {
    FS::IFNode* node;
    size_t length;
    const char* name;
    uint64_t _pad;
};

FS::Response<NPFS::Directory::DirectoryEntry*> NPFS::Directory::FindEntry(const FS::DirectoryEntry& fileref) {
    if (fileref.Name == nullptr || fileref.NameLength == 0) {
        return FS::Response<DirectoryEntry*>(FS::Status::INVALID_PARAMETER);
    }

    auto data = static_cast<DirectoryData*>(container);
    auto node = &(data->data);

    const size_t blockSize = node->QueryBlockSize();

    if (blockSize % sizeof(DirectoryEntry) != 0) {
        return FS::Response<DirectoryEntry*>(FS::Status::DEVICE_ERROR);
    }

    uint8_t* blk = node->GetWeakBlock(0);

    for (size_t i = 0; blk != nullptr; blk = node->GetWeakBlock(++i)) {
        for (size_t j = 0; j < blockSize / sizeof(DirectoryEntry); ++j) {
            DirectoryEntry* ptr = &(reinterpret_cast<DirectoryEntry*>(blk))[j];

            if (ptr->length == fileref.NameLength) {
                if (Utils::memcmp(ptr->name, fileref.Name, fileref.NameLength) == 0) {
                    return FS::Response(ptr);
                }
            }
        }
    }

    return FS::Response<DirectoryEntry*>(FS::Status::NOT_FOUND);
}

NPFS::Directory::Directory(FS::Owner* owner) : FS::Directory(owner) {}

FS::Response<FS::IFNode*> NPFS::Directory::Find(const FS::DirectoryEntry& fileref) {
    Utils::LockGuard _{mut};

    auto result = FindEntry(fileref);

    if (result.CheckError()) {
        return FS::Response<FS::IFNode*>(result.GetError());
    }

    auto node = result.GetValue()->node;

    auto status = node->Open();

    if (status != FS::Status::SUCCESS) {
        return FS::Response<FS::IFNode*>(status);
    }

    return FS::Response(node);
}

FS::Status NPFS::Directory::CreateEntry(const DirectoryEntry* entry) {
    auto status = FindEntry({ .NameLength = entry->length, .Name = entry->name });

    if (!status.CheckError()) {
        return FS::Status::ALREADY_EXISTS;
    }
    else if (status.GetError() != FS::Status::NOT_FOUND) {
        return status.GetError();
    }

    auto data = static_cast<DirectoryData*>(container);
    auto node = &data->data;

    const size_t blockSize = node->QueryBlockSize();

    if (blockSize % sizeof(DirectoryEntry) != 0) {
        return FS::Status::DEVICE_ERROR;
    }

    uint8_t* blk = node->GetWeakBlock(0);

    size_t i = 0;
    for (; blk != nullptr; blk = node->GetWeakBlock(++i)) {
        for (size_t j = 0; j < blockSize / sizeof(DirectoryEntry); ++j) {
            DirectoryEntry* ptr = &(reinterpret_cast<DirectoryEntry*>(blk))[j];

            if (ptr->length == 0) {
                *ptr = *entry;
                return FS::Status::SUCCESS;
            }
        }
    }

    DirectoryEntry* ptr = reinterpret_cast<DirectoryEntry*>(node->GetBlock(i));
    
    if (ptr == nullptr) {
        return FS::Status::DEVICE_ERROR;
    }

    Utils::memset(ptr, 0, blockSize);

    *ptr = *entry;

    return FS::Status::SUCCESS;
}

FS::Status NPFS::Directory::Create(const FS::DirectoryEntry& fileref, FS::FileType type) {
    if (fileref.Name == nullptr) {
        return FS::Status::INVALID_PARAMETER;
    }

    char* nameCopy = static_cast<char*>(Heap::Allocate(fileref.NameLength));

    if (nameCopy == nullptr) {
        return FS::Status::DEVICE_ERROR;
    }

    Utils::memcpy(nameCopy, fileref.Name, fileref.NameLength);

    DirectoryEntry entry;

    if (type == FS::FileType::FILE) {
        void* mem = Heap::Allocate(sizeof(File));

        if (mem == nullptr) {
            Heap::Free(nameCopy);
            return FS::Status::DEVICE_ERROR;
        }

        File* file = new(mem) File(owner);

        if (!File::Construct(file)) {
            Heap::Free(nameCopy);
            Heap::Free(mem);
            return FS::Status::DEVICE_ERROR;
        }

        entry.node = file;
        entry.length = fileref.NameLength;
        entry.name = nameCopy;
    }
    else if (type == FS::FileType::DIRECTORY) {
        void* mem = Heap::Allocate(sizeof(Directory));

        if (mem == nullptr) {
            Heap::Free(nameCopy);
            return FS::Status::DEVICE_ERROR;
        }

        Directory* directory = new(mem) Directory(owner);

        if (!Directory::Construct(directory)) {
            Heap::Free(nameCopy);
            Heap::Free(mem);
            return FS::Status::DEVICE_ERROR;
        }

        entry.node = directory;
        entry.length = fileref.NameLength;
        entry.name = nameCopy;
    }
    else {
        Heap::Free(nameCopy);
        return FS::Status::INVALID_PARAMETER;
    }

    Utils::LockGuard _{mut};

    auto status = CreateEntry(&entry);

    if (status != FS::Status::SUCCESS) {
        Heap::Free(nameCopy);
        Heap::Free(entry.node);
    }

    return status;
}

FS::Status NPFS::Directory::AddNode(const FS::DirectoryEntry& fileref, FS::IFNode* node) {
    if (fileref.Name == nullptr) {
        return FS::Status::INVALID_PARAMETER;
    }

    char* nameCopy = static_cast<char*>(Heap::Allocate(fileref.NameLength));

    if (nameCopy == nullptr) {
        return FS::Status::DEVICE_ERROR;
    }

    Utils::memcpy(nameCopy, fileref.Name, fileref.NameLength);

    DirectoryEntry entry = {
        .node = node,
        .length = fileref.NameLength,
        .name = nameCopy,
        ._pad = 0
    };

    Utils::LockGuard _{mut};

    auto status = CreateEntry(&entry);

    if (status != FS::Status::SUCCESS) {
        Heap::Free(nameCopy);
    }

    return status;
}

FS::Status NPFS::Directory::Remove(const FS::DirectoryEntry& fileref) {
    Utils::LockGuard _{mut};

    auto result = FindEntry(fileref);

    if (result.CheckError()) {
        return result.GetError();
    }

    DirectoryEntry* entry = result.GetValue();

    auto node = entry->node;
    auto status = node->Open();

    if (status == FS::Status::SUCCESS) {
        node->MarkForRemoval();

        Heap::Free(const_cast<char*>(entry->name));
        entry->name = nullptr;
        entry->length = 0;
        entry->node = nullptr;

        node->Close();
    }
    else if (status != FS::Status::UNAVAILABLE) {
        return status;
    }

    return FS::Status::SUCCESS;
}

FS::Response<size_t> NPFS::Directory::List(FS::DirectoryEntry* list, size_t length, size_t from) {
    auto data = static_cast<DirectoryData*>(container);
    auto node = &data->data;

    const size_t blockSize = node->QueryBlockSize();
    const size_t entrySize = sizeof(DirectoryEntry);

    if (blockSize % entrySize != 0) {
        return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
    }

    const size_t entriesPerBlock = blockSize / entrySize;

    size_t blockId = from / entriesPerBlock;

    size_t entryId = from % entriesPerBlock;
    size_t remaining = length;
    uint8_t* blk = nullptr;

    Utils::LockGuard _{mut};

    // name not deep copied within kernel memory;
    // is copied when required by user memory, however

    while ((blk = node->GetWeakBlock(blockId)) != nullptr) {
        for (; entryId < entriesPerBlock && remaining > 0; ++entryId) {
            DirectoryEntry* ptr = reinterpret_cast<DirectoryEntry*>(blk) + entryId;

            if (ptr->length != 0) {
                list->Name = ptr->name;
                list->NameLength = ptr->length;
                ++list;
                --remaining;
            }
        }

        ++blockId;
        entryId = 0;
    }

    return FS::Response(length - remaining);
}

bool NPFS::Directory::Construct(Directory* directory) {
    DirectoryData* data = static_cast<DirectoryData*>(Heap::Allocate(sizeof(DirectoryData)));

    if (data == nullptr) {
        return false;
    }
    else if (!DataNode::Construct(data->data)) {
        Heap::Free(data);
        return false;
    }

    directory->container = data;

    return true;
}

void NPFS::Directory::Destroy() {
    DirectoryData* data = static_cast<DirectoryData*>(container);

    // TODO: mark all files for deletion

    data->data.Destroy();

    Heap::Free(data);
    Heap::Free(this);
}

NPFS::File::File(FS::Owner* owner) : FS::File(owner) {}

FS::Response<size_t> NPFS::File::Read(size_t offset, size_t count, uint8_t* buffer) {
    auto fileinfo = static_cast<FileData*>(container);
    auto data = &fileinfo->data;

    if (offset >= fileinfo->size) {
        return FS::Response<size_t>(0);
    }

    const size_t blockSize = data->QueryBlockSize();
    const size_t blockOffset = offset % blockSize;
    const size_t end = GetEffectiveEnd(offset, count, fileinfo->size);
    const size_t remaining = end % blockSize;

    const size_t firstBlock = offset / blockSize;
    const size_t lastBlock = end / blockSize;

    size_t effectiveCount = end - offset;
    uint8_t* const effectiveBufferEnd = buffer + effectiveCount;

    Utils::LockGuard _{mut};

    if (firstBlock == lastBlock) {
        uint8_t* blk = data->GetWeakBlock(firstBlock);

        if (blk != nullptr) {
            Utils::memcpy(buffer, blk + blockOffset, effectiveCount);
        }

        return FS::Response(effectiveCount);
    }
    else if (blockOffset != 0) {
        const size_t untilNextBlock = blockSize - blockOffset;

        uint8_t* blk = data->GetWeakBlock(firstBlock);

        if (blk != nullptr) {
            for (size_t i = 0; i < untilNextBlock; ++i) {
                Utils::memcpy(buffer, blk + blockOffset, untilNextBlock);
            }
        }

        buffer += untilNextBlock;
        offset += untilNextBlock;
        effectiveCount -= untilNextBlock;
    }

    if (remaining != 0) {
        uint8_t* blk = data->GetWeakBlock(lastBlock);

        if (blk != nullptr) {
            for (size_t i = 1; i <= remaining; ++i) {
                effectiveBufferEnd[0 - i] = blk[end - i];
            }
        }

        effectiveCount -= remaining;
    }

    if (offset % blockSize != 0 || effectiveCount % blockSize != 0) {
        return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
    }

    const size_t readFrom = offset / blockSize;
    const size_t remainingBlocks = effectiveCount / blockSize;

    if (remainingBlocks > 0) {
        for (size_t i = readFrom; i < readFrom + remainingBlocks; ++i) {
            uint8_t* blk = data->GetWeakBlock(i);

            if (blk != nullptr) {
                Utils::memcpy(buffer, blk, blockSize);
            }

            buffer += blockSize;
        }
    }

    return FS::Response<size_t>(effectiveBufferEnd - buffer);
}

FS::Response<size_t> NPFS::File::Write(size_t offset, size_t count, const uint8_t* buffer) {
    auto fileinfo = static_cast<FileData*>(container);
    auto data = &fileinfo->data;
    
    if (offset + count < offset) {
        // cannot do + 1 as it would break everything
        // max size becomes SIZE_MAX - 1
        count = SIZE_MAX - offset;
    }

    const size_t blockSize = data->QueryBlockSize();
    const size_t blockOffset = offset % blockSize;
    const size_t end = offset + count;
    const size_t remaining = end % blockSize;

    const size_t firstBlock = offset / blockSize;
    const size_t lastBlock = end / blockSize;

    const uint8_t* const bufferEnd = buffer + count;

    if (end > fileinfo->size) {
        fileinfo->size = end;
    }

    Utils::LockGuard _{mut};

    if (firstBlock == lastBlock) {
        uint8_t* blk = data->GetBlock(firstBlock);

        if (blk == nullptr) {
            return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
        }

        Utils::memcpy(blk + blockOffset, buffer, count);

        return FS::Response(count);
    }
    else if (blockOffset != 0) {
        const size_t untilNextBlock = blockSize - blockOffset;

        uint8_t* blk = data->GetBlock(firstBlock);

        if (blk == nullptr) {
            return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
        }

        Utils::memcpy(blk + blockOffset, buffer, untilNextBlock);
        
        buffer += untilNextBlock;
        offset += untilNextBlock;
        count -= untilNextBlock;
    }

    if (remaining != 0) {
        uint8_t* blk = data->GetBlock(lastBlock);

        if (blk == nullptr) {
            return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
        }

        for (size_t i = 1; i <= remaining; ++i) {
            blk[end - i] = bufferEnd[0 - i];
        }

        count -= remaining;
    }

    if (offset % blockSize != 0 || count % blockSize != 0) {
        return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
    }

    const size_t writeFrom = offset / blockSize;
    const size_t remainingBlocks = count / blockSize;
    
    if (remainingBlocks > 0) {
        for (size_t i = writeFrom; i < writeFrom + remainingBlocks; ++i) {
            uint8_t* blk = data->GetBlock(i);

            if (blk == nullptr) {
                return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
            }

            Utils::memcmp(blk, buffer, blockSize);
            
            buffer += blockSize;
        }
    }

    return FS::Response<size_t>(bufferEnd - buffer);
}

bool NPFS::File::Construct(File* file) {
    FileData* data = static_cast<FileData*>(Heap::Allocate(sizeof(FileData)));

    if (data == nullptr) {
        return false;
    }
    else if (!DataNode::Construct(data->data)) {
        Heap::Free(data);
        return false;
    }

    data->size = 0;

    file->container = data;

    return true;
}

void NPFS::File::Destroy() {
    FileData* data = static_cast<FileData*>(container);

    data->data.Destroy();

    Heap::Free(data);
    Heap::Free(this);
}

NPFS::NPFS() : root(this) {}

bool NPFS::Construct(NPFS* fs) {
    auto newfs = new(fs) NPFS;

    if (!Directory::Construct(&newfs->root)) {
        return false;
    }

    return true;
}
