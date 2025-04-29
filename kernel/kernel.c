#include <stdint.h>
#include <gdt.h>
#include <kernel.h>
#include <pcfs.h>
#include <vga.h>
#include <idt.h>
#include <ata.h>

/*
 * WARNING:
 * Always open this file at codepage CP437!
*/

void kentr() {
    init_gdt();
    init_idt();
    init_pit();
    kprintf("ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\n");
    kprintf("³ FOCUS Operating System   v1.3  ³\n");
    kprintf("³ Created by Michael Bugaev      ³\n");
    kprintf("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\n");
    init_dmem();
    ata_init();
    vfs_init();
    setup_system_t();

    char *input;
    for (;;)
    {
        kprintf("0:/ -> ");
        get_string(input);
        shell_execute(input);
    }
}
