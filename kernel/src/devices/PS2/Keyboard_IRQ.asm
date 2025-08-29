BITS 64

extern main_core_dump
extern main_core_reload
extern PS2KeyboardEventHandler

global PS2_IRQ1_Handler

section .text
PS2_IRQ1_Handler:
    call main_core_dump
    call PS2KeyboardEventHandler
    sub rsp, 8
    call main_core_reload
    add rsp, 8
    iretq
