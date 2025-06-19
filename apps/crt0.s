global _start
extern main
%define USER_DS 0x23        ; как в enter_user.c

_start:
    mov     ax, USER_DS
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax          ; на всякий случай

    push    ebp
    mov     ebp, esp
    and     esp, 0xFFFFFFF0   ; выравниваем стек на 16
    sub     esp, 8            ; shadow space для GCC (как в SysV ABI)

    call    main              ; вызываем функцию пользователя

    add     esp, 8            ; убираем shadow space

    ; eax = код возврата (0)
    xor     eax, eax
    mov     ebx, eax          ; статус
    mov     eax, 1            ; SYS_exit (enum: 1)
    int     0x80

    hlt                       ; на случай, если ядро не вернуло