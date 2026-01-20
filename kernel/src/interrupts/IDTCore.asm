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

extern IDT_INTERRUPT_DISPATCHER

section .text

isr_routine:
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

    ; load arguments (RCX: stack pointer, RDX: error code, R8: vector number)
    mov rcx, rsp
    mov rdx, [rsp + 16 * 8]
    mov r8, [rsp + 15 * 8]

    ; reserve shadow space
    sub rsp, 32

    call IDT_INTERRUPT_DISPATCHER

    ; reclaim shadow space
    add rsp, 32

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
    add rsp, 16 ; remove alignment and error code
    
    iretq

%macro push_dummy_error 0
    sub rsp, 8
    mov qword [rsp], 0
%endmacro

%macro stub_template_error 1
    sub rsp, 8
    mov qword [rsp], %1
    jmp isr_routine
%endmacro

%macro stub_template 1
    push_dummy_error
    stub_template_error %1
%endmacro

%macro stub 1
stub_%1:
stub_template %1
%endmacro

%macro stub_error 1
stub_%1:
stub_template_error %1
%endmacro

;; define architecture stubs

stub 0          ; DE
stub 1          ; DB
stub 2          ; NMI
stub 3          ; BP
stub 4          ; OF
stub 5          ; BR
stub 6          ; UD
stub 7          ; NM
stub_error 8    ; DF
stub 9          ; CSO
stub_error 10   ; TS
stub_error 11   ; NP
stub_error 12   ; SS
stub_error 13   ; GP
stub_error 14   ; PF
stub 15         ; Reserved
stub 16         ; MF
stub_error 17   ; AC
stub 18         ; MC
stub 19         ; XM
stub 20         ; VE
stub_error 21   ; CP
stub 22         ; Reserved
stub 23         ; Reserved
stub 24         ; Reserved
stub 25         ; Reserved
stub 26         ; Reserved
stub 27         ; Reserved
stub 28         ; HV
stub_error 29   ; VC
stub_error 30   ; SX
stub 31         ; Reserved

;; define generic stubs

%assign stub_n 32
%rep (256 - 32)
stub stub_n
%assign stub_n stub_n + 1
%endrep

section .data

global IDT_STUB_TABLE

IDT_STUB_TABLE:
%assign stub_n 0
%rep 256
dq stub_%+stub_n
%assign stub_n stub_n + 1
%endrep
