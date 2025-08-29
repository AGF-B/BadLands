#pragma once

#include <cstdint>

namespace Ext {
	template<class T> constexpr uint32_t FNV1A32(T x) {
		static constexpr uint32_t FNV_prime = 0x01000193;
		static constexpr uint32_t FNV_offset_basis = 0x811c9dc5;

		uint32_t hash = FNV_offset_basis;

		const uint8_t* const raw = reinterpret_cast<const uint8_t*>(&x);

		for (size_t i = 0; i < sizeof(T); ++i) {
			hash ^= *(raw + i);
			hash *= FNV_prime;
		}

		return hash;
	}

	template<> constexpr uint32_t FNV1A32<const char*>(const char* x) {
		static constexpr uint32_t FNV_prime = 0x01000193;
		static constexpr uint32_t FNV_offset_basis = 0x811c9dc5;

		uint32_t hash = FNV_offset_basis;

		while (*x != 0) {
			hash ^= *x++;
			hash *= FNV_prime;
		}

		return hash;
	}
}
