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
#include <sched.h>

void kentr(uint32_t magic, struct multiboot_info *mbi) {
    qemu_debug_printf("Kernel entry point\n");
    qemu_debug_printf("Multiboot magic: 0x%08x\n", magic);
    qemu_debug_printf("Multiboot address: 0x%08x\n", mbi->framebuffer_addr);
    qemu_debug_printf("VBE width: %d, height: %d\n", mbi->framebuffer_width, mbi->framebuffer_height);
    init_gdt();
    init_idt();
    scheduler_init();
    init_paging();
    qemu_debug_printf("IDT initialized\n");
    // Initialize local APIC and APIC timer instead of PIT
    // May be work slow on some PCs but it's faster than that shit
    apic_init();
    // Start APIC timer: period value to generate ~1000 ticks per second (calibrate as needed)
    apic_timer_init(1000000, APIC_TIMER_MODE_PERIODIC, APIC_TIMER_VECTOR);
    qemu_debug_printf("APIC timer initialized\n");
    init_dmem();
    vbe_init(mbi);
    kprint("Starting FOCUS kernel Ring 0...\n");
    // Enumerate PCI devices
    pci_enumerate();
    //draw_pixel(fb, 0, 0, 0x0F);
    ata_init();
    atapi_init();
    fat32_mount(0);
    iso9660_mount(2);
    kprintf("flib: kprintf at 0x%08x\n", &kprintf);
    kprintf("flib: kprint at 0x%08x\n", &kprint);

    net_init();
    int bmp;
    uint8_t *bmp_data = malloc(219 * 87);
    if (!bmp_data) kprintf("can't print logo. error while allocating memory\n");
    else {
        bmp = iso9660_read("cdrom:/focus/images/logo.bmp", bmp_data, 219 * 87);
        if (bmp <= 0) kprintf("can't print logo. error while reading file. sz: %d\n", bmp);
        else {
            //print_bmp16(bmp_data, 100, 42, 0, 0);
        }
    }

    if (bmp > 0) print_bmp16(bmp_data, 219, 87, 800 - 230, 5);
    mfree(bmp_data);
    kprintf("RING 2 DONE\n");




    kprintf("KERNEL END. HALT.");
    for (;;);
}