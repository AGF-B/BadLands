#pragma once

#include <shared/efi/efi.h>

namespace Runtime {
	void Initialize();
	const EFI_RUNTIME_SERVICES* GetServices();
}
