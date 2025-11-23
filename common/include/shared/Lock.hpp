#pragma once

#include <shared/SimpleAtomic.hpp>

namespace Utils {
    class Lock{
    public:
        inline void lock() noexcept {
            bool desired = true;
            bool expected = false;

            while (!pvt_lock.compare_exchange(
                expected,
                desired
            )) { expected = false; }
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
