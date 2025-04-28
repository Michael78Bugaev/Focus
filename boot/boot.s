.section .multiboot
.align 8
multiboot_header:
    .long 0xE85250D6           # magic
    .long 0                    # architecture (protected)
    .long multiboot_header_end - multiboot_header # header length
    .long -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header)) # checksum

    # Framebuffer tag (type 5)
    .align 8
    .short 5                    # tag type: framebuffer
    .short 0                    # reserved
    .long 20                    # size of this tag
    .long 800                   # width
    .long 600                   # height
    .long 32                    # depth (bits per pixel)

    # End tag
    .align 8
    .short 0
    .short 0
    .long 8

multiboot_header_end:

.section .text
.global _start
.extern kmain

_start:
    mov $stack_top, %esp
    mov %ebx, %eax
    push %eax
    call kmain
    cli
    hlt

.section .bss
    .align 4
stack_bottom:
    .skip 16384
stack_top: