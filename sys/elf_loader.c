// elf_loader.c — загрузка ELF-файла

#include "elf.h"
#include <string.h>
#include "elf_loader.h"
#include <stdio.h>
#include <paging.h>
#include <stddef.h>

extern void *kmalloc_aligned(uint32_t size, uint32_t align);
static void memcpy_to_vaddr_manual(Elf32_Addr vaddr, const void *src, size_t len);

/* map one 4-MiB page (identity) and mark Present|RW */
static void map_4M(uint32_t vaddr)
{
    uint32_t pd_idx = vaddr >> 22;
    extern uint32_t *page_directory;   /* из paging.c */
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        uint32_t phys = pd_idx << 22;                /* identity     */
        page_directory[pd_idx] = phys | PAGE_PRESENT | PAGE_RW | (1<<7);
        asm volatile ("invlpg (%0)" :: "r"(vaddr));  /* flush TLB    */
    }
}

/* ensure every 4-MiB chunk in [vaddr, vaddr+memsz) mapped */
static void ensure_region_mapped(uint32_t vaddr, uint32_t memsz)
{
    uint32_t start = vaddr & ~0x3FFFFF;              /* 4 МиБ align  */
    uint32_t end   = (vaddr + memsz + 0x3FFFFF) & ~0x3FFFFF;
    for (uint32_t p = start; p < end; p += 0x400000)
        map_4M(p);
}

// Загрузка сегментов PT_LOAD в память
void elf_load_segments(const uint8_t *elf_data, void (*memcpy_to_vaddr)(Elf32_Addr vaddr, const void *src, size_t len)) {
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)elf_data;
    const Elf32_Phdr *phdrs = (const Elf32_Phdr *)(elf_data + ehdr->e_phoff);

    kprintf("[ELF] Loading %d segments\n", ehdr->e_phnum);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const Elf32_Phdr *ph = &phdrs[i];
        kprintf("[ELF] Segment %d: type=%u vaddr=0x%08x offset=0x%08x filesz=%u memsz=%u\n", i, ph->p_type, ph->p_vaddr, ph->p_offset, ph->p_filesz, ph->p_memsz);
        if (ph->p_type != PT_LOAD) continue;

        ensure_region_mapped(ph->p_vaddr, ph->p_memsz);
        memcpy_to_vaddr_manual(ph->p_vaddr, &elf_data + 0x1000, ph->p_filesz);
        kprintf("[ELF]   -> copied %u bytes to 0x%08x\n", ph->p_filesz, ph->p_vaddr);

        if (ph->p_memsz > ph->p_filesz) {
            memset((void *)(uintptr_t)(ph->p_vaddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
            kprintf("[ELF]   -> BSS: %u bytes at 0x%08x\n", ph->p_memsz - ph->p_filesz, ph->p_vaddr + ph->p_filesz);
        }
    }
}

// Пример функции memcpy_to_vaddr для bare-metal/OS
void memcpy_to_vaddr(Elf32_Addr vaddr, const void *src, size_t len) {
    memcpy(src, (void *)vaddr, len);
}

static void memcpy_to_vaddr_manual(Elf32_Addr vaddr, const void *src, size_t len)
{
    uint8_t *dst = (uint8_t*)vaddr;
    const uint8_t *s = (const uint8_t*)src;
    for(size_t i=0;i<len;i++) dst[i]=s[i];
}

// Основная функция загрузки ELF
bool elf_load(const uint8_t *elf_data, void (*memcpy_to_vaddr)(Elf32_Addr, const void *, size_t), Elf32_Addr *entry_point) {
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)elf_data;
    kprintf("[ELF] Checking ELF header...\n");
    if (!elf_check_file(ehdr)) {
        kprintf("[ELF] Error: invalid ELF header (bad magic): %s\n", ehdr->e_ident);
        return false;
    }
    kprintf("[ELF] Checking format support...\n");
    if (!elf_check_supported(ehdr)) {
        kprintf("[ELF] Error: unsupported file (not 32-bit ELF/x86)\n");
        return false;
    }
    kprintf("[ELF] Loading segments...\n");
    elf_load_segments(elf_data, memcpy_to_vaddr);
    if (entry_point) {
        *entry_point = ehdr->e_entry;
        kprintf("[ELF] Entry point: 0x%08x\n", ehdr->e_entry);
    }
    kprintf("[ELF] ELF load successful\n");
    return true;
}

int elf_load_image(const uint8_t *file, uint32_t size, uint32_t user_stack_top, void (**entry_out)(void)) {
    (void)size;
    Elf32_Addr entry = 0;
    kprintf("[ELF] elf_load_image started, user_stack_top=0x%08x\n", user_stack_top);
    int res = elf_load(file, memcpy_to_vaddr_manual, &entry);
    if (!res) {
        kprintf("[ELF] ELF load error\n");
        return -1;
    }
    if (entry_out) *entry_out = (void (*)(void))entry;
    kprintf("[ELF] First 16 bytes at entry: ");
    for(int i=0;i<16;i++) kprintf("%02X ", ((uint8_t*)entry)[i]);
    kprintf("\n");
    kprintf("[ELF] Done, entry=0x%08x\n", entry);
    return 0;
}