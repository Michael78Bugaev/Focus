#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1    // Page is present in memory
#define PAGE_RW      0x2    // Page is read/write
#define PAGE_USER    0x4    // Page is user-accessible
#define PAGE_PWT    0x8    // Page-level write-through
#define PAGE_PCD    0x10   // Page-level cache disable

// Initialize paging (identity 4-GiB mapping, 4-MiB pages)
void init_paging(void);

void disable_cache_for_region(uint32_t addr, uint32_t size);

#endif // PAGING_H 