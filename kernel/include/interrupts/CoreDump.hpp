#pragma once

#include <cstdint>

namespace Panic {
	void DumpCore(void* panic_stack, uint64_t errv);
}
