global _start
extern main

_start:
    push    ebp
    mov     ebp, esp
    and     esp, 0xFFFFFFF0   ; выравниваем стек на 16
    sub     esp, 8            ; shadow space для GCC (как в SysV ABI)

    call    main              ; вызываем функцию пользователя

    add     esp, 8            ; убираем shadow space
    mov     esp, ebp
    pop     ebp

    ret                       ; вернуть управление оболочке (ядру)