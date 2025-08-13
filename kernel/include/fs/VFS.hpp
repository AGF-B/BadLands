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
