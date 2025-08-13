#pragma once

#include <cstddef>

namespace Heap {
	bool Create(); 
	void* Allocate(size_t size);
	void Free(void* ptr);
}
