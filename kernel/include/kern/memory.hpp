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

#include <cstdint>

#include <new>
#include <type_traits>
#include <utility>

#include <shared/SimpleAtomic.hpp>

#include <mm/Heap.hpp>
#include <mm/IOHeap.hpp>
#include <mm/MemoryProvider.hpp>

namespace kern {
    template<class T, MemoryProvider Provider = Heap>
    class unique_ptr {
    private:
        T* ptr;

        using X = typename std::remove_extent<T>::type;

        constexpr void ReleaseMemory() {
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
        
        inline constexpr ~unique_ptr() { ReleaseMemory(); }

        inline constexpr T* release() {
            T* tmp = ptr;
            ptr = nullptr;
            return tmp;
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;
        inline constexpr unique_ptr& operator=(unique_ptr&& other) {
            if (this != &other) {
                ReleaseMemory();
                ptr = other.ptr;
                other.ptr = nullptr;
            }

            return *this;
        }

        inline constexpr T* operator->() const { return ptr; }
        inline constexpr T& operator*() const { return *ptr; }
        inline constexpr operator bool() const { return ptr != nullptr; }

        inline constexpr size_t size() const = delete;
    };

    template<class T, MemoryProvider Provider>
    class unique_ptr<T[], Provider> {
    private:
        T* ptr;
        size_t length;

        constexpr void ReleaseMemory() {
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
        
        inline constexpr ~unique_ptr() { ReleaseMemory(); }

        inline constexpr T* release() {
            T* tmp = ptr;
            ptr = nullptr;
            length = 0;
            return tmp;
        }

        inline constexpr T* get() { return ptr; }
        inline constexpr T* get() const { return ptr; }

        inline constexpr unique_ptr& operator=(const unique_ptr&) = delete;
        inline constexpr unique_ptr& operator=(unique_ptr&& other) {
            if (this != &other) {
                ReleaseMemory();

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

        inline constexpr size_t size() const { return length; }

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
    inline constexpr unique_ptr<T, Provider> make_unique(size_t i) requires std::is_array_v<T> {
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
    inline constexpr unique_io_ptr<T> make_unique_io(size_t i) requires std::is_array_v<T> {
        return make_unique<T, IOHeap>(i);
    }

    template<class T, MemoryProvider Provider = Heap, class... Args>
    inline constexpr unique_ptr<T, Provider> make_unique(Args&&... args) requires (!std::is_array_v<T>) {
        auto* ptr = static_cast<T*>(Provider::Allocate(sizeof(T)));

        if (ptr == nullptr) {
            return unique_ptr<T, Provider>();
        }

        new (ptr) T(std::forward<Args>(args)...);

        return unique_ptr<T, Provider>(ptr);
    }

    template<class T, class... Args>
    inline constexpr unique_io_ptr<T> make_unique_io(Args&&... args) {
        return make_unique<T, IOHeap>(std::forward<Args>(args)...);
    }

    class BasicCountedObject {
    public:
        Utils::SimpleAtomic<size_t> references;
        void (*deleter)(BasicCountedObject*);
    };

    template<class T, MemoryProvider Provider = Heap>
    class shared_ptr {
    public:
        class CountedInlinePtr : public BasicCountedObject {
        public:
            alignas(T) uint8_t container[sizeof(T)];

            static void Destroyer(BasicCountedObject* obj) {
                auto* self = static_cast<CountedInlinePtr*>(obj);
                reinterpret_cast<T*>(self->container)->~T();
                self->~CountedInlinePtr();
                Provider::Free(self);
            }

            inline constexpr CountedInlinePtr() {
                this->references.store(0);
                this->deleter = &Destroyer;
            }
        };

    private:
        BasicCountedObject* control;
        T* ptr;

        inline constexpr void ReleaseMemory() {
            if (control != nullptr) {
                if (--control->references == 0) {
                    auto tmp_control = control;
                    control = nullptr;
                    ptr = nullptr;
                    tmp_control->deleter(tmp_control);
                }
            }
        }

        inline constexpr shared_ptr(BasicCountedObject* control, T* ptr) : control{control}, ptr{ptr} {}

        template<class U, MemoryProvider P, class... Args>
        friend constexpr shared_ptr<U, P> make_shared(Args&&... args);

        template<class V, class U, MemoryProvider P>
        friend constexpr shared_ptr<V, P> static_pointer_cast(const shared_ptr<U, P>& other);

    public:
        inline constexpr shared_ptr() : control{nullptr}, ptr{nullptr} {}
        inline constexpr shared_ptr(const shared_ptr& other) : control{other.control}, ptr{other.ptr} {
            if (control != nullptr) {
                ++control->references;
            }
        }
        inline constexpr shared_ptr(shared_ptr&& other) : control{other.control}, ptr{other.ptr} {
            other.control = nullptr;
            other.ptr = nullptr;
        }

        inline constexpr ~shared_ptr() { ReleaseMemory(); }

        inline constexpr T* get() { return ptr; }
        inline constexpr T* get() const { return ptr; }

        inline constexpr shared_ptr& operator=(const shared_ptr& other) {
            if (this != &other) {
                ReleaseMemory();

                control = other.control;
                ptr = other.ptr;

                if (control != nullptr) {
                    ++control->references;
                }
            }

            return *this;
        }
        inline constexpr shared_ptr& operator=(shared_ptr&& other) {
            if (this != &other) {
                ReleaseMemory();

                control = other.control;
                ptr = other.ptr;

                other.control = nullptr;
                other.ptr = nullptr;
            }

            return *this;
        }

        inline constexpr T* operator->() const { return ptr; }
        inline constexpr T& operator*() const { return *ptr; }
        inline constexpr operator bool() const { return ptr != nullptr; }
    };

    /// TODO: Implement shared_ptr<T[]> and make_shared<T[]>(size_t)
    template<typename T>
    class shared_ptr<T[]> {
        static_assert(sizeof(T) == 0, "shared_ptr<T[]> is not yet implemented");
    };

    template<class T, MemoryProvider Provider = Heap, class... Args>
    inline constexpr shared_ptr<T, Provider> make_shared(Args&&... args) {
        using control_t = shared_ptr<T, Provider>::CountedInlinePtr;

        auto* control = static_cast<control_t*>(
            Provider::Allocate(sizeof(control_t))
        );

        if (control == nullptr) {
            return shared_ptr<T, Provider>();
        }

        new (control) control_t();
        new (control->container) T(std::forward<Args>(args)...);
        control->references.store(1);

        return shared_ptr<T, Provider>(control, reinterpret_cast<T*>(control->container));
    }

    template<class V, class U, MemoryProvider P>
    inline constexpr shared_ptr<V, P> static_pointer_cast(const shared_ptr<U, P>& other) {
        if (other.control != nullptr) {
            ++other.control->references;
        }
        return shared_ptr<V, P>(other.control, static_cast<V*>(other.ptr));
    }
}
