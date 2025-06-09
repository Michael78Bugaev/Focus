#include <stdint.h>
#include <gdt.h>
#include <vga.h>
#include <multiboot.h>
#include <idt.h>
#include <fat32.h>
#include <paging.h>
#include <vbe_terminal.h>
/*
 * WARNING:
 * Always open this file at codepage CP437!
*/



void kentr(uint32_t magic, struct multiboot_info *mbi) {
    qemu_debug_printf("Kernel entry point\n");
    qemu_debug_printf("Multiboot magic: 0x%08x\n", magic);
    qemu_debug_printf("Multiboot address: 0x%08x\n", mbi->framebuffer_addr);
    init_gdt();
    init_idt();
    qemu_debug_printf("IDT initialized\n");
    init_pit();
    qemu_debug_printf("PIT initialized\n");
    init_dmem();
    vbe_init(mbi);
    //draw_pixel(fb, 0, 0, 0x0F);
    ata_init();
    atapi_init();
    shell_execute("fatmount");
    shell_execute("isomount");
    kprintf("Press any key to continue...\n");
    kgetch();
    kclear();
    kprintf("<(05)>�OCUS<(07)> Operating System v1.5 <(0f)>\n<(0a)>Copyright MIT v3.0 License<(0f)>\n");
    kprintf("Created by Michael Bugaev\n\n");
    kprintf("%c FCSASM Compiler\n", 0x1a);
    kprintf("%c FAT32 File System\n\n", 0x1a);
    kprintf("Project: <(0b)>https://github.com/Michael78Bugaev/Focus/tree/master<(0f)> \nWritten in C and Assembly. Enjoy!\n");

    char *input;
    for (;;)
    {
        print_prompt();
        get_string(input);
        shell_execute(input);
    }
}

void print_prompt() {
    extern uint32_t current_dir_cluster;
    extern uint32_t root_dir_first_cluster;
    if (current_dir_cluster == root_dir_first_cluster) {
        kprint("0:\\>");
    } else {
        char path[8][9]; // до 8 вложенных директорий
        int depth = build_path(current_dir_cluster, path, 8);
        kprint("0:");
        for (int i = depth - 1; i >= 0; i--) {
            kprint("\\");
            kprint(path[i]);
        }
        kprint(">");
    }
}