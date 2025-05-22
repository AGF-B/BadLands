BITS 64

%define SETUP_OK                 0
%define ERROR_CPUID_UNSUPPORTED -1
%define ERROR_NXE_UNSUPPORTED   -2
%define PAT_UNSUPPORTED         -3

%define CPUID_PAT               0x00000001
%define CPUID_CAPABILITIES      0x80000000
%define CPUID_NXE               0x80000001
%define CPUID_ADDRESSING        0x80000008

%define CPUID_NXE_MASK          0x00100000
%define EFER_MSR                0xC0000080
%define EFER_NXE_MASK           0x00000800
%define MAXPHYADDR_MASK         0x000000FF
%define CPUID_PAT_MASK          0x00010000
%define IA32_PAT_MSR            0x00000277
%define PAT_WC                  0x01

section .text
global EfiLoaderSetup

; RCX: pointer to output variable meant to store the value of the unsupported CPUID command if any
; RDX: pointer to output variable meant to store the value of the physical address width
EfiLoaderSetup:
    mov r10, rcx
    mov r11, rdx
; force-enables SSE
    mov rax, cr0
    and ax, 0xFFFB
    or ax, 0x2
    mov cr0, rax
    mov rax, cr4
    or ax, (3 << 9)
    mov cr4, rax
; loads CPUID capabilities
    mov eax, CPUID_CAPABILITIES
    cpuid
    mov r8d, eax

; prepares NXE bit
    ; checks if CPUID supports command 0x80000001
    mov ecx, CPUID_NXE
    cmp r8d, ecx
    mov eax, ERROR_CPUID_UNSUPPORTED
    jl cpuid_return
    mov eax, CPUID_NXE
    cpuid
    ; check if CPU supports NXE bit in EFER
    and edx, CPUID_NXE_MASK
    mov eax, ERROR_NXE_UNSUPPORTED
    je return
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_NXE_MASK
    wrmsr

; recovers the physical address width
    ; checks if CPUID supports command 0x80000008
    mov ecx, CPUID_ADDRESSING
    cmp r8d, ecx
    mov eax, ERROR_CPUID_UNSUPPORTED
    jl cpuid_return
    mov eax, CPUID_ADDRESSING
    cpuid
    and eax, MAXPHYADDR_MASK
    mov [r11], al

; programs PAT4 in the PAT MSR in order to set up Write Combining
    mov eax, CPUID_PAT
    cpuid
    and edx, CPUID_PAT_MASK
    test edx, edx
    jne .L0
    mov rax, PAT_UNSUPPORTED
    jmp return
.L0:
    mov ecx, IA32_PAT_MSR
    rdmsr
    mov dl, PAT_WC
    wrmsr
    mov eax, SETUP_OK
return:
    ret

cpuid_return:
    mov [r10], ecx
    ret