// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <new>
#include <type_traits>
#include <utility>

#include <mm/Heap.hpp>

namespace kern {
    template<class T>
    class unique_ptr {
    private:
        T* ptr;

        using X = typename std::remove_extent<T>::type;

    public:
        inline constexpr unique_ptr() : ptr {nullptr} {}
        inline constexpr unique_ptr(T* ptr) : ptr {ptr} {}
        inline constexpr unique_ptr(const unique_ptr&) = delete;
        inline constexpr unique_ptr(unique_ptr&& other) : ptr{other.ptr} {
            other.ptr = nullptr;
        }
        
        inline constexpr ~unique_ptr() {
            if (ptr != nullptr) {
                ptr->~T();
                Heap::Free(ptr);
            }
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr const T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;

        inline constexpr T* operator->() const { return ptr; }
        inline constexpr T& operator*() const { return *ptr; }
        inline constexpr operator bool() const { return ptr != nullptr; }
    };

    template<class T>
    class unique_ptr<T[]> {
    private:
        T* ptr;
        size_t length;

    public:
        inline constexpr unique_ptr() : ptr{nullptr}, length{0} {}
        inline constexpr unique_ptr(T* ptr, size_t length) : ptr {ptr}, length{length} {}
        inline constexpr unique_ptr(const unique_ptr&) = delete;
        inline constexpr unique_ptr(unique_ptr&& other) : ptr{other.ptr}, length{other.length} {
            other.ptr = nullptr;
            other.length = 0;
        }
        
        inline constexpr ~unique_ptr() {
            if (ptr != nullptr) {
                for (size_t i = 0; i < length; ++i) {
                    ptr[i].~T();
                }

                Heap::Free(ptr);
            }
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr const T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;

        inline constexpr T* operator->() = delete;
        inline constexpr T& operator*() = delete;
        inline constexpr operator bool() const { return ptr != nullptr; }

        inline constexpr T& operator[](size_t i) {
            return ptr[i];
        }

        inline constexpr const T& operator[](size_t i) const {
            return ptr[i];
        }
    };

    template<class T>
    constexpr unique_ptr<T> make_unique(size_t i) requires std::is_array_v<T> {
        using X = typename std::remove_extent<T>::type;
        
        auto* ptr = static_cast<X*>(Heap::Allocate(sizeof(X) * i));

        if (ptr == nullptr) {
            return unique_ptr<T>();
        }

        new (ptr) T();

        return unique_ptr<T>(ptr, i);
    }

    template<class T, class... Args>
    constexpr unique_ptr<T> make_unique(Args&&... args) {
        auto* ptr = static_cast<T*>(Heap::Allocate(sizeof(T)));

        if (ptr == nullptr) {
            return unique_ptr<T>();
        }

        new (ptr) T(std::forward<Args>(args)...);

        return unique_ptr<T>(ptr);
    }
}
