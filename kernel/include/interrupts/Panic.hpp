#pragma once

#include <cstdint>

#include <shared/efi/efi.h>

namespace Panic {
	[[noreturn]] void Panic(const char* msg);
	[[noreturn]] void Panic(const char* msg, uint64_t errv);
	[[noreturn]] void PanicShutdown( const char* msg);
}
