#pragma once

#include <shared/Lock.hpp>

namespace Utils {
    class LockGuard {
    private:
        Lock& lock;
    public:
        LockGuard(Lock& lock) : lock{lock} {
            lock.lock();
        }

        ~LockGuard() {
            lock.unlock();
        }
    };
}
