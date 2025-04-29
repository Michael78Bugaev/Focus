BITS 32

section .multiboot
    ALIGN 4
    DD 0x1BADB002
    DD 0 ; VBE - 5
    DD -(0x1BADB002 + 0) ; VBE - 5
    dd 0
    dd 0
    dd 0
    dd 0
    dw 5
    dw 0 ;instead of 0, you can specify your flags
    dd 0
    dd 640 ;instead of 1024, you can specify your width
    dd 480 ;instead of 768, you can specify your height
    dd 32 ;instead of 32, you can specify your BPP

global start
global stack_space
extern kentr
extern vga_80x50
extern enter_real_mode
extern enter_protected_mode

section .text

push ebx

start:
    CLI
    MOV esp, stack_space
    PUSH ebx
    PUSH eax
    CALL kentr
    HLT
HaltKernel:
    HLT
    JMP HaltKernel

section .bss
RESB 8192
stack_space: