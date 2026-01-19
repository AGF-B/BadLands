#pragma once

#include <type_traits>

#include <shared/MemoryOrdering.hpp>

extern "C" void __blatomic_store_1(volatile uint8_t* ptr, uint8_t val, Utils::MemoryOrder order);
extern "C" void __blatomic_store_2(volatile uint16_t* ptr, uint16_t val, Utils::MemoryOrder order);
extern "C" void __blatomic_store_4(volatile uint32_t* ptr, uint32_t val, Utils::MemoryOrder order);
extern "C" void __blatomic_store_8(volatile uint64_t* ptr, uint64_t val, Utils::MemoryOrder order);

extern "C" uint8_t  __blatomic_load_1(const volatile uint8_t* ptr, Utils::MemoryOrder order);
extern "C" uint16_t __blatomic_load_2(const volatile uint16_t* ptr, Utils::MemoryOrder order);
extern "C" uint32_t __blatomic_load_4(const volatile uint32_t* ptr, Utils::MemoryOrder order);
extern "C" uint64_t __blatomic_load_8(const volatile uint64_t* ptr, Utils::MemoryOrder order);

extern "C" uint8_t  __blatomic_exchange_1(volatile uint8_t* ptr, uint8_t val, Utils::MemoryOrder order);
extern "C" uint16_t __blatomic_exchange_2(volatile uint16_t* ptr, uint16_t val, Utils::MemoryOrder order);
extern "C" uint32_t __blatomic_exchange_4(volatile uint32_t* ptr, uint32_t val, Utils::MemoryOrder order);
extern "C" uint64_t __blatomic_exchange_8(volatile uint64_t* ptr, uint64_t val, Utils::MemoryOrder order);

extern "C" bool __blatomic_compare_exchange_1(volatile uint8_t* ptr, uint8_t* expected, uint8_t desired);
extern "C" bool __blatomic_compare_exchange_2(volatile uint16_t* ptr, uint16_t* expected, uint16_t desired);
extern "C" bool __blatomic_compare_exchange_4(volatile uint32_t* ptr, uint32_t* expected, uint32_t desired);
extern "C" bool __blatomic_compare_exchange_8(volatile uint64_t* ptr, uint64_t* expected, uint64_t desired);

extern "C" uint8_t __blatomic_add_fetch_1(volatile uint8_t* ptr, uint8_t val, Utils::MemoryOrder order);
extern "C" uint16_t __blatomic_add_fetch_2(volatile uint16_t* ptr, uint16_t val, Utils::MemoryOrder order);
extern "C" uint32_t __blatomic_add_fetch_4(volatile uint32_t* ptr, uint32_t val, Utils::MemoryOrder order);
extern "C" uint64_t __blatomic_add_fetch_8(volatile uint64_t* ptr, uint64_t val, Utils::MemoryOrder order);

extern "C" uint8_t __blatomic_sub_fetch_1(volatile uint8_t* ptr, uint8_t val, Utils::MemoryOrder order);
extern "C" uint16_t __blatomic_sub_fetch_2(volatile uint16_t* ptr, uint16_t val, Utils::MemoryOrder order);
extern "C" uint32_t __blatomic_sub_fetch_4(volatile uint32_t* ptr, uint32_t val, Utils::MemoryOrder order);
extern "C" uint64_t __blatomic_sub_fetch_8(volatile uint64_t* ptr, uint64_t val, Utils::MemoryOrder order);

namespace Utils {
    template<typename T> concept atomic_t = std::is_integral_v<T> || std::is_pointer_v<T>;

    template<atomic_t T, MemoryOrder Order = MemoryOrder::SEQ_CST>
    class SimpleAtomic {
    private:
        T atomic_value;
        
        SimpleAtomic(const SimpleAtomic&) = delete;
        SimpleAtomic(SimpleAtomic&&) = delete;
        SimpleAtomic& operator=(const SimpleAtomic&) = delete;
        SimpleAtomic& operator=(SimpleAtomic&&) = delete;

    public:
        inline constexpr SimpleAtomic() : atomic_value{0} {}
        inline constexpr SimpleAtomic(T v) : atomic_value{v} {}

        inline T load(MemoryOrder order = Order) const volatile {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_load_1(
                    reinterpret_cast<const volatile uint8_t*>(&atomic_value),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_load_2(
                    reinterpret_cast<const volatile uint16_t*>(&atomic_value),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_load_4(
                    reinterpret_cast<const volatile uint32_t*>(&atomic_value),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_load_8(
                    reinterpret_cast<const volatile uint64_t*>(&atomic_value),
                    order
                ));
            }
        }

        inline void store(T v, MemoryOrder order = Order) volatile {
            if constexpr (sizeof(T) == 1) {
                __blatomic_store_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    static_cast<uint8_t>(v),
                    order
                );
            }
            else if constexpr (sizeof(T) == 2) {
                __blatomic_store_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    static_cast<uint16_t>(v),
                    order
                );
            }
            else if constexpr (sizeof(T) == 4) {
                __blatomic_store_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    static_cast<uint32_t>(v),
                    order
                );
            }
            else if constexpr (sizeof(T) == 8) {
                __blatomic_store_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    static_cast<uint64_t>(v),
                    order
                );
            }
        }

        inline T operator=(T v) volatile {
            store(v);
            return v;
        }

        inline operator T() const volatile {
            return load();
        }

        inline T operator--() volatile {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_sub_fetch_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_sub_fetch_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_sub_fetch_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_sub_fetch_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
        }

        inline constexpr operator++() volatile {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_add_fetch_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_add_fetch_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_add_fetch_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_add_fetch_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    1,
                    Order
                ));
            }
        }

        inline constexpr operator+=(const T& addend) {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_add_fetch_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    addend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_add_fetch_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    addend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_add_fetch_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    addend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_add_fetch_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    addend,
                    Order
                ));
            }
        }

        inline constexpr operator-=(const T& subtrahend) {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_sub_fetch_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    subtrahend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_sub_fetch_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    subtrahend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_sub_fetch_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    subtrahend,
                    Order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_sub_fetch_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    subtrahend,
                    Order
                ));
            }
        }

        inline bool compare_exchange(T& expected, T desired, [[maybe_unused]] MemoryOrder order = Order) volatile {
            if constexpr (sizeof(T) == 1) {
                return __blatomic_compare_exchange_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    reinterpret_cast<uint8_t*>(&expected),
                    static_cast<uint8_t>(desired)
                );
            }
            else if constexpr (sizeof(T) == 2) {
                return __blatomic_compare_exchange_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    reinterpret_cast<uint16_t*>(&expected),
                    static_cast<uint16_t>(desired)
                );
            }
            else if constexpr (sizeof(T) == 4) {
                return __blatomic_compare_exchange_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    reinterpret_cast<uint32_t*>(&expected),
                    static_cast<uint32_t>(desired)
                );
            }
            else if constexpr (sizeof(T) == 8) {
                return __blatomic_compare_exchange_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    reinterpret_cast<uint64_t*>(&expected),
                    static_cast<uint64_t>(desired)
                );
            }
        }

        inline T exchange(T v, MemoryOrder order = Order) volatile {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(__blatomic_exchange_1(
                    reinterpret_cast<volatile uint8_t*>(&atomic_value),
                    static_cast<uint8_t>(v),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(__blatomic_exchange_2(
                    reinterpret_cast<volatile uint16_t*>(&atomic_value),
                    static_cast<uint16_t>(v),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(__blatomic_exchange_4(
                    reinterpret_cast<volatile uint32_t*>(&atomic_value),
                    static_cast<uint32_t>(v),
                    order
                ));
            }
            else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(__blatomic_exchange_8(
                    reinterpret_cast<volatile uint64_t*>(&atomic_value),
                    static_cast<uint64_t>(v),
                    order
                ));
            }
        }
    };
}
