.global idt_load
idt_load:
    lidt (%eax)
    ret
