section .multiboot
align 8
multiboot_header:
    dd 0xE85250D6              ; magic
    dd 0                       ; architecture (protected)
    dd multiboot_header_end - multiboot_header ; header length
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header)) ; checksum

    ; Framebuffer tag (type 5)
    align 8
    dw 5                       ; tag type: framebuffer
    dw 0                       ; reserved
    dd 20                      ; size of this tag
    dd 800                     ; width
    dd 600                     ; height
    dd 32                      ; depth (bits per pixel)

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8

multiboot_header_end:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    mov eax, ebx
    push eax
    call kernel_main
    cli
    hlt

section .bss
align 4
stack_bottom:
    resb 16384
stack_top: 