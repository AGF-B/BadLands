#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

namespace Runtime {
	static const EFI_RUNTIME_SERVICES* rtServices = nullptr;
	
	void Initialize() {
		rtServices = *reinterpret_cast<EFI_RUNTIME_SERVICES**>(
			Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.RTServices
		);
	}

	const EFI_RUNTIME_SERVICES* GetServices() {
		return rtServices;
	}
}
