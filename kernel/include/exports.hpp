#pragma once

#include <fs/VFS.hpp>

namespace Kernel {
    struct KernelExports {
        VFS* vfs;
        FS::IFNode* deviceInterface;
    };

    extern KernelExports Exports;
}
