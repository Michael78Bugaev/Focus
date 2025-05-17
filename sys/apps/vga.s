BITS 32
org 0

_start:
    mov esi, msg         ; Адрес строки
    mov edi, 0xB8000     ; Адрес VGA памяти
.next_char:
    lodsb                ; AL = [ESI], ESI++
    test al, al
    jz .done
    mov ah, 0x0F         ; Атрибут (белый на чёрном)
    mov [edi], ax        ; Пишем символ и атрибут
    add edi, 2
    jmp .next_char
.done:
    jmp $

msg db 'Hello, FocusOS!', 0