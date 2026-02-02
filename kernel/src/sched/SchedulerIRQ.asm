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

extern SCHEDULER_IRQ_DISPATCHER

section .text
global SCHEDULER_IRQ_HANDLER
global SCHEDULER_SOFT_IRQ_HANDLER

SCHEDULER_IRQ_HANDLER:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdx, rsp

    sub rsp, 16
    mov qword [rsp], 0
    mov qword [rsp + 8], 0

    mov rcx, rsp
    mov r8, 1 ; is_timer_irq = true

    ; reserve shadow space
    sub rsp, 32

    call SCHEDULER_IRQ_DISPATCHER

    ; reclaim shadow space and return value memory
    add rsp, 32

    mov rax, [rsp]
    mov rcx, [rsp + 8]

    add rsp, 16

    cmp rax, 0
    jz .no_switch

    mov cr3, rax
    mov rsp, rcx

.no_switch:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    
    iretq

SCHEDULER_SOFT_IRQ_HANDLER:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdx, rsp

    sub rsp, 16
    mov qword [rsp], 0
    mov qword [rsp + 8], 0

    mov rcx, rsp
    mov r8, 0 ; is_timer_irq = false

    ; reserve shadow space
    sub rsp, 32

    call SCHEDULER_IRQ_DISPATCHER

    ; reclaim shadow space and return value memory
    add rsp, 32

    mov rax, [rsp]
    mov rcx, [rsp + 8]

    add rsp, 16

    cmp rax, 0
    jz .no_switch

    mov cr3, rax
    mov rsp, rcx

.no_switch:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    
    iretq