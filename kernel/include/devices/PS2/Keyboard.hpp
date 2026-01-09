#pragma once

#include <fs/IFNode.hpp>

namespace Devices {
	namespace PS2 {
		enum class StatusCode {
			SUCCESS,
			FATAL_ERROR
		};

		StatusCode InitializeKeyboard(FS::IFNode* keyboardMultiplexer);
	}
}
