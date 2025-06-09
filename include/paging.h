#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1    // Page is present in memory
#define PAGE_RW      0x2    // Page is read/write
#define PAGE_USER    0x4    // Page is user-accessible

// Initialize 32-bit paging with identity mapping for the first 4 MiB
void init_paging(void);

#endif // PAGING_H 