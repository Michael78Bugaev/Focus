/*
 * Simple identity-mapping paging using 4 MiB pages (PSE).
 * Every virtual address maps to the same physical address, so
 * existing flat-kernel code continues to work, but now we have
 * ability to apply page protection, create guard-pages
 * for stack and etc.
 *
 *  – page directory occupies 4 KiB and is aligned.
 *  – all 1024 PDE are filled: 4 GiB / 4 MiB = 1024.
 *  – PSE (bit 4 CR4) is enabled, and in PDE we put PS (bit 7).
 */

#include <paging.h>
#include <mem.h>

uint32_t page_directory[1024] __attribute__((aligned(4096)));

void init_paging(void)
{
    /* 1. Fill PDE: base physical address and flags */
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t phys = i * 0x400000;          /* 4 MiB */
        page_directory[i] = phys | PAGE_PRESENT | PAGE_RW | (1 << 7); /* PS=1 */
    }

    /* 2. Load PD address to CR3 (paging disabled ⇒ VA=PA). */
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    /* 3. Enable PSE (Intel® 3A) – bit4 CR4 */
    uint32_t cr4;
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010;        /* PSE */
    asm volatile ("mov %0, %%cr4" :: "r"(cr4));

    /* 4. Enable paging (bit31 PG in CR0) */
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;        /* PG */
    asm volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void disable_cache_for_region(uint32_t addr, uint32_t size)
{
    /* Align start and end to 4 MiB page boundary */
    uint32_t start = addr & 0xFFC00000;               /* clear lower 22 bits */
    uint32_t end   = (addr + size - 1) & 0xFFC00000;
    for (uint32_t p = start; p <= end; p += 0x400000) {
        uint32_t idx = p >> 22;                       /* PDE index */
        page_directory[idx] |= PAGE_PCD;              /* disable caching */
    }
    /* Reset TLB by reloading CR3 */
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));
} 
