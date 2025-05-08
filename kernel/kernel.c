#include <stdint.h>
#include <gdt.h>
#include <vga.h>
#include <idt.h>
#include <fat32.h>

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
    shell_execute("fatmount");

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
        kprint("0:\>");
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