BITS 16

start:
    mov ax, 0x7c00
    mov ds, ax
    mov si, 0x4E

loop:
    lodsb
    or al, al
    jz hang
    mov ah, 0x0E
    mov bx, 0x0007
    int 10h
    jmp loop
hang:
    hlt
    jmp $