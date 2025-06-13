global _start
extern main

; very small startup code for FocusOS FEX
; We assume flat 32-bit, interrupts enabled, stack already valid.
_start:
    mov esp, 0x003FF000 ; set a private stack (top at 4MB-4K)
    call main        ; transfer control
    ret               ; give control back to the shell when main exits 