BITS 64

extern SCHEDULER_IRQ_DISPATCHER

section .text
global SCHEDULER_IRQ_HANDLER

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
