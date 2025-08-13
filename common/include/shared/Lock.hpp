#pragma once

#include <atomic>

namespace Utils {
    class Lock{
    public:
        inline void lock() noexcept {
            bool desired = true;
            bool expected = false;

            while (!pvt_lock.compare_exchange_strong(
                expected,
                desired
            ));
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
        std::atomic<bool> pvt_lock{false};
    };
}
