#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1    // Page is present in memory
#define PAGE_RW      0x2    // Page is read/write
#define PAGE_USER    0x4    // Page is user-accessible

// Initialize paging (identity 4-GiB mapping, 4-MiB pages)
void init_paging(void);

#endif // PAGING_H 