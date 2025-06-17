#include <stdint.h>
#include <gdt.h>
#include <vga.h>
#include <multiboot.h>
#include <idt.h>
#include <fat32.h>
#include <paging.h>
#include <vbe_terminal.h>
#include <apic.h>
#include <pci.h>
/*
 * WARNING:
 * Always open this file at codepage CP437!
*/

void kentr(uint32_t magic, struct multiboot_info *mbi) {
    qemu_debug_printf("Kernel entry point\n");
    qemu_debug_printf("Multiboot magic: 0x%08x\n", magic);
    qemu_debug_printf("Multiboot address: 0x%08x\n", mbi->framebuffer_addr);
    qemu_debug_printf("VBE width: %d, height: %d\n", mbi->framebuffer_width, mbi->framebuffer_height);
    init_gdt();
    init_idt();
    init_paging();
    qemu_debug_printf("IDT initialized\n");
    // Initialize local APIC and APIC timer instead of PIT
    apic_init();
    // Start APIC timer: period value to generate ~1000 ticks per second (calibrate as needed)
    apic_timer_init(1000000, APIC_TIMER_MODE_PERIODIC, APIC_TIMER_VECTOR);
    qemu_debug_printf("APIC timer initialized\n");
    init_dmem();
    vbe_init(mbi);
    kprint("Starting FOCUS kernel...\n");
    // Enumerate PCI devices
    pci_enumerate();
    //draw_pixel(fb, 0, 0, 0x0F);
    ata_init();
    atapi_init();
    shell_execute("fatmount");
    shell_execute("isomount 2");
    kprintf("flib: kprintf at 0x%08x\n", &kprintf);
    kprintf("flib: kprint at 0x%08x\n", &kprint);

    net_init();
    int bmp;
    uint8_t *bmp_data = malloc(219 * 87);
    if (!bmp_data)
    {
        kprintf("Can't print logo. Error while allocating memory\n");
    }
    else
    {
        bmp = iso9660_read("cdrom:/focus/images/logo.bmp", bmp_data, 219 * 87);
        if (bmp <= 0)
        {
            kprintf("Can't print logo. Error while reading file. sz: %d\n", bmp);
        }
        else {
            //print_bmp16(bmp_data, 100, 42, 0, 0);
        }
    }
    kprintf("Starting /focus/init.fcs...\n");
    shell_execute_fsc("cdrom:/focus/init.fcs");

    if (bmp > 0) print_bmp16(bmp_data, 219, 87, 800 - 230, 5);
    mfree(bmp_data);

    for (;;);
}

void print_prompt() {
    extern uint32_t current_dir_cluster;
    extern uint32_t root_dir_first_cluster;
    if (current_dir_cluster == root_dir_first_cluster) {
        kprint("0:\\>");
    } else {
        char path[32][9];
        int depth = build_path(current_dir_cluster, path, 32);
        kprint("0:");
        for (int i = depth - 1; i >= 0; i--) {
            kprint("\\");
            kprint(path[i]);
        }
        kprint(">");
    }
}