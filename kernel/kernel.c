#include <stdint.h>
#include <gdt.h>
#include <kernel.h>
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
        kprintf("0:\\> ");
    } else {
        fat32_dir_entry_t entries[32];
        int n = fat32_read_dir(0, root_dir_first_cluster, entries, 32);
        for (int i = 0; i < n; i++) {
            if ((entries[i].attr & 0x10) == 0x10) {
                uint32_t cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                if (cl == current_dir_cluster) {
                    char name[9];
                    int pos = 0;
                    for (int j = 0; j < 8; j++) {
                        if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) name[pos++] = entries[i].name[j];
                    }
                    name[pos] = 0;
                    kprintf("0:\\%s> ", name);
                    return;
                }
            }
        }
        kprintf("0:\\?> ");
    }
}
