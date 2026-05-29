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
#include <mm/IOHeap.hpp>
#include <mm/MemoryProvider.hpp>

namespace kern {
    template<class T, MemoryProvider Provider = Heap>
    class unique_ptr {
    private:
        T* ptr;

        using X = typename std::remove_extent<T>::type;

        constexpr void Release() {
            if (ptr != nullptr) {
                ptr->~T();
                Provider::Free(ptr);
            }
        }

    public:
        using _Provider = Provider;

        inline constexpr unique_ptr() : ptr {nullptr} {}
        inline constexpr unique_ptr(T* ptr) : ptr {ptr} {}
        inline constexpr unique_ptr(const unique_ptr&) = delete;
        inline constexpr unique_ptr(unique_ptr&& other) : ptr{other.ptr} {
            other.ptr = nullptr;
        }
        
        inline constexpr ~unique_ptr() { Release(); }

        inline constexpr T* release() {
            T* tmp = ptr;
            ptr = nullptr;
            return tmp;
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr const T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;
        inline constexpr unique_ptr& operator=(unique_ptr&& other) {
            if (this != &other) {
                Release();
                ptr = other.ptr;
                other.ptr = nullptr;
            }

            return *this;
        }

        inline constexpr T* operator->() const { return ptr; }
        inline constexpr T& operator*() const { return *ptr; }
        inline constexpr operator bool() const { return ptr != nullptr; }
    };

    template<class T, MemoryProvider Provider>
    class unique_ptr<T[], Provider> {
    private:
        T* ptr;
        size_t length;

        constexpr void Release() {
            if (ptr != nullptr) {
                for (size_t i = 0; i < length; ++i) {
                    ptr[i].~T();
                }

                Provider::Free(ptr);
            }
        }

    public:
        using _Provider = Provider;

        inline constexpr unique_ptr() : ptr{nullptr}, length{0} {}
        inline constexpr unique_ptr(T* ptr, size_t length) : ptr {ptr}, length{length} {}
        inline constexpr unique_ptr(const unique_ptr&) = delete;
        inline constexpr unique_ptr(unique_ptr&& other) : ptr{other.ptr}, length{other.length} {
            other.ptr = nullptr;
            other.length = 0;
        }
        
        inline constexpr ~unique_ptr() { Release(); }

        inline constexpr T* release() {
            T* tmp = ptr;
            ptr = nullptr;
            length = 0;
            return tmp;
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr const T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;
        inline constexpr unique_ptr& operator=(unique_ptr&& other) {
            if (this != &other) {
                Release();

                ptr = other.ptr;
                length = other.length;

                other.ptr = nullptr;
                other.length = 0;
            }

            return *this;
        }

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
    using unique_io_ptr = unique_ptr<T, IOHeap>;

    template<class T, MemoryProvider Provider = Heap>
    constexpr unique_ptr<T, Provider> make_unique(size_t i) requires std::is_array_v<T> {
        using X = typename std::remove_extent<T>::type;
        
        auto* ptr = static_cast<X*>(Provider::Allocate(sizeof(X) * i));

        if (ptr == nullptr) {
            return unique_ptr<T, Provider>();
        }

        for (size_t j = 0; j < i; ++j) {
            new (ptr + j) X();
        }

        return unique_ptr<T, Provider>(ptr, i);
    }

    template<class T>
    constexpr unique_io_ptr<T> make_unique_io(size_t i) requires std::is_array_v<T> {
        return make_unique<T, IOHeap>(i);
    }

    template<class T, MemoryProvider Provider = Heap, class... Args>
    constexpr unique_ptr<T, Provider> make_unique(Args&&... args) {
        auto* ptr = static_cast<T*>(Provider::Allocate(sizeof(T)));

        if (ptr == nullptr) {
            return unique_ptr<T, Provider>();
        }

        new (ptr) T(std::forward<Args>(args)...);

        return unique_ptr<T, Provider>(ptr);
    }

    template<class T, class... Args>
    constexpr unique_io_ptr<T> make_unique_io(Args&&... args) {
        return make_unique<T, IOHeap>(std::forward<Args>(args)...);
    }
}
