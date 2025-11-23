#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace Log {
	void Setup();

	void putcAt(char c, uint32_t x, uint32_t y);
	void putc(char c);
	void puts(const char* s);
	void vprintf(const char* format, va_list args);
	void printf(const char* format, ...);

	void putAtSafe(char c, uint32_t x, uint32_t y);
	void putcSafe(char c);
	void putsSafe(const char* s);
	void vprintfSafe(const char* format, va_list args);
	void printfSafe(const char* format, ...);
}
