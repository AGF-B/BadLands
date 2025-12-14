#pragma once

#include <cstdint>

#include <type_traits>

template<typename T> concept integral_t = std::is_integral_v<T>;

template<integral_t T>
constexpr T& ModifyPacked(T& target, T mask, uint8_t shift, T value) {
    target = (target & ~mask) | ((value << shift) & mask);
    return target;
}

template<integral_t T, integral_t R>
constexpr T& ModifyPacked(T& target, T mask, uint8_t shift, R value) {
    return ModifyPacked<T>(target, mask, shift, static_cast<T>(value));
}

template<integral_t T>
constexpr T GetPacked(const T& source, T mask, uint8_t shift) {
    return (source & mask) >> shift;
}

template<integral_t T, integral_t R>
constexpr R GetPacked(const T& source, T mask, uint8_t shift) {
    return static_cast<R>(GetPacked<T>(source, mask, shift));
}
