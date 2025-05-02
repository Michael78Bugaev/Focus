#include <stdint.h>
#include <gdt.h>
#include <kernel.h>
#include <vga.h>
#include <idt.h>

/*
 * WARNING:
 * Always open this file at codepage CP437!
*/

void kentr() {
    init_gdt();
    init_idt();
    init_pit();
    kprintf("FOCUS Operating System   v1.3\n");
    kprintf("Created by Michael Bugaev\n");
    init_dmem();
    
    ata_init();

    char *input;
    for (;;)
    {
        kprintf("0:/> ");
        get_string(input);
        shell_execute(input);
    }
}
