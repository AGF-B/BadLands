#pragma once

#include <fs/IFNode.hpp>
#include <fs/VFS.hpp>

namespace Devices {
	namespace KeyboardDispatcher {
		FS::IFNode* Initialize(FS::IFNode* deviceInterface);
	}
}
