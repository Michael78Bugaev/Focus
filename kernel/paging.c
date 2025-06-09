#include <paging.h>

// Aligned arrays for page directory and first-level page table
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));

void init_paging(void) {
    // Disable interrupts during paging setup
    asm volatile("cli");
    // Identity-map the first 4 MiB of memory using 4 KiB pages
    for (uint32_t i = 0; i < 1024; i++) {
        first_page_table[i] = (i << 12) | PAGE_PRESENT | PAGE_RW;
    }

    // Setup page directory: entry 0 points to our first page table
    page_directory[0] = (uint32_t)first_page_table | PAGE_PRESENT | PAGE_RW;
    // Mark other page directory entries as not present
    for (uint32_t i = 1; i < 1024; i++) {
        page_directory[i] = 0;
    }
    qemu_debug_printf("Loading page directory into CR3\n");
    // Load the page directory into CR3
    asm volatile("mov %0, %%cr3" :: "r" (page_directory));
    // Enable paging by setting the PG bit (bit 31) in CR0
    qemu_debug_printf("Enabling paging\n");
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r" (cr0));
    qemu_debug_printf("CR0 before: 0x%08x\n", cr0);
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r" (cr0));
    qemu_debug_printf("CR0 after: 0x%08x\n", cr0);
    // Re-enable interrupts now that paging is active
    asm volatile("sti");
} 