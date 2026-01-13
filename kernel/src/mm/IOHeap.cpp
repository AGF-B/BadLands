#include <cstddef>
#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>

#include <mm/IOHeap.hpp>
#include <mm/VirtualMemory.hpp>

namespace ShdMem = Shared::Memory;

namespace IOHeap {
    static constexpr size_t DEFAULT_ARENA_SIZE = 16 * 1024 * 1024; // 16 MB
    static constexpr size_t DEFAULT_PAGES = DEFAULT_ARENA_SIZE / ShdMem::PAGE_SIZE;
    
    struct Metadata {
        uint32_t padding;
        uint32_t size;
        Metadata* next;
    };

    static Metadata* head = nullptr;

    static Utils::Lock heapLock;

    Success Create() {
        if (head == nullptr) {
            head = static_cast<Metadata*>(VirtualMemory::AllocateKernelHeap(DEFAULT_PAGES, true));

            if (head == nullptr) {
                return Failure();
            }

            head->padding = 0;
            head->size = DEFAULT_ARENA_SIZE - sizeof(Metadata);
            head->next = nullptr;
        }

        return Success();
    }

    void* Allocate(const size_t size, const size_t alignment) {
        if ((alignment > ShdMem::PAGE_SIZE) || (alignment % 8 != 0)) {
            return nullptr;
        }
        
        Utils::LockGuard _{heapLock};

        Metadata* prev = nullptr;
        Metadata* best_fit_prev = nullptr;
        Metadata* best_fit = nullptr;

        for (Metadata* current = head; current != nullptr; prev = current, current = current->next) {
            size_t total_size = current->size;
            size_t padding = 0;

            uintptr_t address = reinterpret_cast<uintptr_t>(current) + sizeof(Metadata);

            if (address % alignment != 0) {
                padding = alignment - (address % alignment);
            }

            if (padding + size <= total_size) {
                if (best_fit == nullptr || current->size < best_fit->size) {
                    best_fit_prev = prev;
                    best_fit = current;
                    best_fit->padding = static_cast<uint32_t>(padding);
                }
            }
        }
        
        if (best_fit != nullptr) {
            size_t allocated_size = size + best_fit->padding;

            if (allocated_size < best_fit->size - sizeof(Metadata)) {
                Metadata* new_metadata = reinterpret_cast<Metadata*>(
                    reinterpret_cast<uint8_t*>(best_fit) + sizeof(Metadata) + allocated_size
                );

                new_metadata->size = best_fit->size - allocated_size - sizeof(Metadata);
                new_metadata->padding = 0;
                new_metadata->next = best_fit->next;

                best_fit->size = static_cast<uint32_t>(allocated_size);
                best_fit->next = new_metadata;
            }
            else {
                allocated_size = best_fit->size;
            }

            if (best_fit_prev == nullptr) {
                head = best_fit->next;
            }
            else {
                best_fit_prev->next = best_fit->next;
            }

            best_fit->next = nullptr;

            Metadata* user_metadata = reinterpret_cast<Metadata*>(
                reinterpret_cast<uintptr_t>(best_fit) + best_fit->padding
            );

            *user_metadata = *best_fit;

            return user_metadata + 1;
        }

        return nullptr;
    }

    static void Coalesce(Metadata* current) {
        Metadata* const next = current->next;

        if (next != nullptr) {
            const uintptr_t current_end = reinterpret_cast<uintptr_t>(current) + sizeof(Metadata) + current->size;
            const uintptr_t next_address = reinterpret_cast<uintptr_t>(next);

            if (current_end == next_address) {
                current->size += sizeof(Metadata) + next->size;
                current->next = next->next;
            }
        }
    }

    void Free(void* ptr) {
        if (ptr != nullptr) {
            Utils::LockGuard _{heapLock};

            Metadata* user_metadata = reinterpret_cast<Metadata*>(ptr) - 1;

            Metadata* metadata = reinterpret_cast<Metadata*>(
                reinterpret_cast<uint8_t*>(user_metadata) - user_metadata->padding
            );

            for (Metadata* current = head; current != nullptr; current = current->next) {
                const uintptr_t current_address = reinterpret_cast<uintptr_t>(current);
                const uintptr_t current_end = current_address + sizeof(Metadata) + current->size;
                const uintptr_t metadata_address = reinterpret_cast<uintptr_t>(metadata);

                Metadata* next = current->next;

                if (next == nullptr) {
                    current->next = metadata;
                }
                else {
                    const uintptr_t next_address = reinterpret_cast<uintptr_t>(next);

                    if (metadata_address >= current_end && metadata_address < next_address) {
                        metadata->next = next;
                        current->next = metadata;
                    }
                }

                Coalesce(current);
            }
        }
    }
}

