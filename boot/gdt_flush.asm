[bits 32]

global gdt_flush
extern gp

gdt_flush:
    mov eax, [esp + 4]    ; Загружаем адрес GDT из параметра
    lgdt [eax]            ; Загружаем новую GDT

    mov ax, 0x10          ; 0x10 - смещение для сегмента данных (третий элемент в GDT)
    mov ds, ax            ; Устанавливаем сегментные регистры
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush       ; 0x08 - смещение для сегмента кода (второй элемент в GDT)
flush:
    push ebp              ; Сохраняем старый кадр стека
    mov ebp, esp          ; Устанавливаем новый кадр стека
    mov esp, ebp          ; Восстанавливаем стек
    pop ebp
    ret                   ; Возвращаемся в вызывающий код

global tss_flush
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret