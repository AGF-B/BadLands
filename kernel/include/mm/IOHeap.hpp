#pragma once

#include <cstddef>

#include <shared/Response.hpp>

namespace IOHeap {
    Success Create(); 
    void* Allocate(size_t size, size_t alignment = 8);
    void Free(void* ptr);
}
