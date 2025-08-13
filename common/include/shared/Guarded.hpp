#pragma once

#include <shared/Lock.hpp>

namespace Utils {
    template<class T> class Guarded{
    public:
        inline Guarded(T initialValue) : value(initialValue) {};

        inline void lock() noexcept {
            mut.lock();
        }

        inline void unlock() noexcept {
            mut.unlock();
        }

        inline T& access() noexcept {
            return value;
        }

        inline Mutex& operator=(const Mutex& m) {
            if (this != &m) {
                value = m.value;
                mut.unlock();
            }

            return *this;
        }

    private:
        T value;
        Mutex mut;
    };
}
