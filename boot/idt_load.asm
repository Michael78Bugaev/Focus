[bits 32]

global idt_load
extern idtp

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret 