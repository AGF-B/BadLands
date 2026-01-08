#pragma once

#include <fs/VFS.hpp>

namespace Kernel {
    struct KernelExports {
        VFS* vfs;
        FS::IFNode* deviceInterface;
        FS::IFNode* keyboardMultiplexerInterface;
    };

    extern KernelExports Exports;
}
