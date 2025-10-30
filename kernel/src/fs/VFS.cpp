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

FS::Status VFS::HandleApplicationPath(const FS::DirectoryEntry& filepath, FS::DirectoryEntry& current, FS::IFNode*& node) {
    const char applicationBase[] = "partitions";

    current.NameLength = 0;

    if (IsApplicationPath(filepath)) {
        auto result = node->Find({ .NameLength = sizeof(applicationBase) - 1, .Name = applicationBase });

        if (result.CheckError()) {
            return result.GetError();
        }

        node->Close();
        node = result.GetValue();

        current.Name = filepath.Name + 1;
    }
    else {
        current.Name = filepath.Name + 2;
    }

    return FS::Status::SUCCESS;
}

FS::Response<FS::DirectoryEntry> VFS::ExtractFileName(const FS::DirectoryEntry& filepath) {
    if (filepath.Name == nullptr || filepath.NameLength == 0 || filepath.NameLength > FS::MAX_FILE_PATH) {
        return FS::Response<FS::DirectoryEntry>(FS::Status::INVALID_PARAMETER);
    }

    FS::DirectoryEntry filename = { .NameLength = 0, .Name = filepath.Name + filepath.NameLength - 1 };

    if (filename.Name[0] == '/') {
        if (filepath.NameLength == 1) {
            return FS::Response(filename);
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

    return FS::Response(filename);
}

bool VFS::Construct(VFS* fs) {
    auto newfs = new(fs) VFS;

    if (!NPFS::Directory::Construct(&newfs->root)) {
        return false;
    }

    return true;
}

#include <screen/Log.hpp>

FS::Response<FS::IFNode*> VFS::OpenParent(const FS::DirectoryEntry& filepath, FS::DirectoryEntry& filename) {
    if (!CheckFilePath(filepath)) {
        return FS::Response<FS::IFNode*>(FS::Status::INVALID_PARAMETER);
    }

    auto extracted = ExtractFileName(filepath);

    if (extracted.CheckError()) {
        return FS::Response<FS::IFNode*>(extracted.GetError());
    }

    filename = extracted.GetValue();

    const size_t parentPathLength = (size_t)(filename.Name - filepath.Name);
    FS::DirectoryEntry parentpath = { .NameLength = parentPathLength, .Name = filepath.Name };

    auto status = root.Open();

    if (status != FS::Status::SUCCESS) {
        return FS::Response<FS::IFNode*>(status);
    }

    FS::IFNode* node = &root;
    FS::DirectoryEntry current;

    if ((status = HandleApplicationPath(parentpath, current, node)) != FS::Status::SUCCESS) {
        root.Close();
        return FS::Response<FS::IFNode*>(status);
    }

    for (size_t i = (size_t)(current.Name - parentpath.Name); i < parentpath.NameLength; ++i) {
        const char c = parentpath.Name[i];

        if (c == '/') {
            if (current.NameLength == 0) {
                node->Close();
                return FS::Response<FS::IFNode*>(FS::Status::INVALID_PARAMETER);
            }

            auto result = node->Find(current);

            node->Close();
            
            if (result.CheckError()) {
                return FS::Response<FS::IFNode*>(result.GetError());
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
        return FS::Response(node);
    }
    
    return FS::Response(node->Find(current));
}

FS::Response<FS::IFNode*> VFS::Open(const FS::DirectoryEntry& filepath) {
    FS::DirectoryEntry filename;

    auto result = OpenParent(filepath, filename);

    if (result.CheckError() || filename.NameLength == 0) {
        return result;
    }

    auto node = result.GetValue();

    result = node->Find(filename);

    node->Close();

    return result;
}
