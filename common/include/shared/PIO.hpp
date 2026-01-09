#pragma once

#include <cstdint>

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %%dx, %%al" : "=a"(ret) : "d"(port));
    return ret;
}

inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %%dx, %%ax" : "=a"(ret) : "d"(port));
    return ret;
}

inline uint32_t indw(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %%dx, %%eax" : "=a"(ret) : "d"(port));
    return ret;
}

inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %%al, %%dx" :: "a"(data), "d"(port));
}

inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %%ax, %%dx" :: "a"(data), "d"(port));
}

inline void outdw(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %%eax, %%dx" :: "a"(data), "d"(port));
}
