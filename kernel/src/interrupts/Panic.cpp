#include <cstddef>
#include <cstdint>

#include <shared/efi/efi.h>

#include <interrupts/CoreDump.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/RuntimeSvc.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace {
	[[noreturn]] static void kernelPanicShutdownFailed() {
		Log::putsSafe("------ KERNEL PANIC (MAXIMAL SEVERITY) ------\n\r");
		Log::putsSafe("Software shutdown failed, please perform a hard reset manually\n\r");
		Log::putsSafe("Press the power button for an extended period of time.\n\r");
		while (1);
	}

	[[noreturn]] static void kernelPanicShutdownSecondary(const EFI_RUNTIME_SERVICES* rtServices) {
		Log::putsSafe("KERNEL PANIC (HIGH SEVERITY): COULD NOT GET CURRENT TIME\n\r");
		Log::putsSafe("Switching to secondary method, shutting down soon...\n\r");

		// should be enough ticks to show the message
		for (size_t i = 0; i < 80000000; ++i) {
			__asm__ volatile("outb %b0, %w1" :: "a"(0), "Nd"(0x80) : "memory");
		}

		rtServices->ResetSystem(EfiResetShutdown, EFI_ABORTED, 0, nullptr);
		kernelPanicShutdownFailed();
	}
}

namespace Panic {
    [[noreturn]] void Panic(const char* msg) {
        Log::putsSafe("\n\r------ KERNEL PANIC ------\n\r");

        if (msg != nullptr) {
            Log::putsSafe("\t\t ");
            Log::putsSafe(msg);
            Log::putsSafe("\n\r");
        }

        UnattachedSelf::ForceHalt();
    }

	[[noreturn]] void Panic(void* panic_stack, const char* msg, uint64_t errv) {
		Log::putsSafe("\n\r------ KERNEL PANIC ------\n\r");

		if (msg != nullptr) {
			Log::putsSafe("\t\t ");
			Log::putsSafe(msg);
			Log::putsSafe("\n\r");
		}

		Panic::DumpCore(panic_stack, errv);

		UnattachedSelf::ForceHalt();
	}


    [[noreturn]] void PanicShutdown(const char* msg) {
		auto* rtServices = Runtime::GetServices();

		Log::putsSafe("------ KERNEL PANIC SHUTDOWN ------\n\r");
		Log::putsSafe("\tREASON: ");
		Log::putsSafe(msg);
		Log::putsSafe("\n\rShutting down in 10 seconds...\n\r");

		EFI_TIME time1, time2;
		EFI_STATUS status = rtServices->GetTime(&time1, nullptr);
		if (status != EFI_SUCCESS) {
			kernelPanicShutdownSecondary(rtServices);
		}

		uint64_t elapsed = 0;

		while (elapsed < 10) {
			if (elapsed % 2 == 0) {
				status = rtServices->GetTime(&time2, nullptr);
				if (status != EFI_SUCCESS) {
					kernelPanicShutdownSecondary(rtServices);
				}
				else if (time1.Second != time2.Second) {
					++elapsed;
				}
			}
			else {
				status = rtServices->GetTime(&time1, nullptr);
				if (status != EFI_SUCCESS) {
					kernelPanicShutdownSecondary(rtServices);
				}
				else if (time1.Second != time2.Second) {
					++elapsed;
				}
			}
		}

		rtServices->ResetSystem(EfiResetShutdown, EFI_ABORTED, 0, nullptr);
		kernelPanicShutdownSecondary(rtServices);
	}
}