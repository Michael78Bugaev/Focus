#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024

typedef struct {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t unused     : 7;
    uint32_t frame      : 20;
} __attribute__((packed)) page_t;

typedef struct {
    page_t pages[PAGE_ENTRIES];
} page_table_t;

typedef struct {
    page_table_t* tables[PAGE_ENTRIES];
    uint32_t tablesPhysical[PAGE_ENTRIES];
    uint32_t physicalAddr;
} page_directory_t;

void paging_init();

#endif 