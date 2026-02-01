;; SPDX-License-Identifier: GPL-3.0-only
;;
;; Copyright (C) 2026 Alexandre Boissiere
;; This file is part of the BadLands operating system.
;;
;; This program is free software: you can redistribute it and/or modify it under the terms of the
;; GNU General Public License as published by the Free Software Foundation, version 3.
;; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
;; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
;; See the GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License along with this program.
;; If not, see <https://www.gnu.org/licenses/>. 

BITS 64

section .text

%define RELAXED 0
%define CONSUME 1
%define ACQUIRE 2
%define RELEASE 3
%define ACQ_REL 4
%define SEQ_CST 5

global __blatomic_store_1
global __blatomic_store_2
global __blatomic_store_4
global __blatomic_store_8

__blatomic_store_1:
    cmp r8, SEQ_CST
    je .seq_cst_store_1

    mov byte [rcx], dl
    ret

.seq_cst_store_1:
    xchg byte [rcx], dl
    ret

__blatomic_store_2:
    cmp r8, SEQ_CST
    je .seq_cst_store_2

    mov word [rcx], dx
    ret

.seq_cst_store_2:
    xchg word [rcx], dx
    ret

__blatomic_store_4:
    cmp r8, SEQ_CST
    je .seq_cst_store_4

    mov dword [rcx], edx
    ret

.seq_cst_store_4:
    xchg dword [rcx], edx
    ret

__blatomic_store_8:
    cmp r8, SEQ_CST
    je .seq_cst_store_8

    mov qword [rcx], rdx
    ret

.seq_cst_store_8:
    xchg qword [rcx], rdx
    ret


global __blatomic_load_1
global __blatomic_load_2
global __blatomic_load_4
global __blatomic_load_8

__blatomic_load_1:
    cmp r8, SEQ_CST
    je .seq_cst_load_1

    mov al, byte [rcx]
    ret

.seq_cst_load_1:
    mov al, byte [rcx]
    mfence
    ret

__blatomic_load_2:
    cmp r8, SEQ_CST
    je .seq_cst_load_2

    mov ax, word [rcx]
    ret

.seq_cst_load_2:
    mov ax, word [rcx]
    mfence
    ret

__blatomic_load_4:
    cmp r8, SEQ_CST
    je .seq_cst_load_4

    mov eax, dword [rcx]
    ret

.seq_cst_load_4:
    mov eax, dword [rcx]
    mfence
    ret

__blatomic_load_8:
    cmp r8, SEQ_CST
    je .seq_cst_load_8

    mov rax, qword [rcx]
    ret

.seq_cst_load_8:
    mov rax, qword [rcx]
    mfence
    ret

global __blatomic_exchange_1
global __blatomic_exchange_2
global __blatomic_exchange_4
global __blatomic_exchange_8

__blatomic_exchange_1:
    xchg byte [rcx], dl
    movzx rax, dl
    ret

__blatomic_exchange_2:
    xchg word [rcx], dx
    movzx rax, dx
    ret

__blatomic_exchange_4:
    xchg dword [rcx], edx
    mov rax, rdx
    ret

__blatomic_exchange_8:
    xchg qword [rcx], rdx
    mov rax, rdx
    ret

global __blatomic_compare_exchange_1
global __blatomic_compare_exchange_2
global __blatomic_compare_exchange_4
global __blatomic_compare_exchange_8

__blatomic_compare_exchange_1:
    mov al, [rdx]
    lock cmpxchg byte [rcx], r8b
    jne .fail1
    mov eax, 1
    ret

.fail1:
    xchg byte [rdx], al
    xor eax, eax
    ret

__blatomic_compare_exchange_2:
    mov ax, [rdx]
    lock cmpxchg word [rcx], si
    jne .fail2
    mov eax, 1
    ret

.fail2:
    xchg word [rdx], ax
    xor eax, eax
    ret

__blatomic_compare_exchange_4:
    mov eax, [rdx]
    lock cmpxchg dword [rcx], r8d
    jne .fail4
    mov eax, 1
    ret

.fail4:
    xchg dword [rdx], eax
    xor eax, eax
    ret

__blatomic_compare_exchange_8:
    mov rax, [rdx]
    lock cmpxchg qword [rcx], r8
    jne .fail8
    mov eax, 1
    ret

.fail8:
    xchg qword [rdx], rax
    xor eax, eax
    ret


global __blatomic_add_fetch_1
global __blatomic_add_fetch_2
global __blatomic_add_fetch_4
global __blatomic_add_fetch_8

__blatomic_add_fetch_1:
    mov al, dl
    lock xadd byte [rcx], al
    add al, dl
    ret

__blatomic_add_fetch_2:
    mov ax, dx
    lock xadd word [rcx], ax
    add ax, dx
    ret

__blatomic_add_fetch_4:
    mov eax, edx
    lock xadd dword [rcx], eax
    add eax, edx
    ret

__blatomic_add_fetch_8:
    mov rax, rdx
    lock xadd qword [rcx], rax
    add rax, rdx
    ret

global __blatomic_sub_fetch_1
global __blatomic_sub_fetch_2
global __blatomic_sub_fetch_4
global __blatomic_sub_fetch_8

__blatomic_sub_fetch_1:
    neg dl
    jmp __blatomic_add_fetch_1

__blatomic_sub_fetch_2:
    neg dx
    jmp __blatomic_add_fetch_2

__blatomic_sub_fetch_4:
    neg edx
    jmp __blatomic_add_fetch_4

__blatomic_sub_fetch_8:
    neg rdx
    jmp __blatomic_add_fetch_8
