#pragma once

#include <shared/SimpleAtomic.hpp>

namespace Utils {
    class Lock{
    public:
        inline bool trylock() noexcept {
            static constexpr bool desired = true;
            bool expected = false;

            if (!pvt_lock.compare_exchange(expected, desired)) {
                return false;
            }

            return true;
        }

        inline void lock() noexcept {
            while (!trylock()) { __asm__ volatile("pause"); }
        }

        inline void unlock() noexcept {
            pvt_lock.store(false);
        }

        inline Lock& operator=(const Lock& m) {
            if (this != &m) {
                pvt_lock.store(false);
            }

            return *this;
        }

    private:
        SimpleAtomic<bool> pvt_lock{false};
    };
}
