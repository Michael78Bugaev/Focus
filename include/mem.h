#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NULL_POINTER ((void*)0)
#define DYNAMIC_MEM_TOTAL_SIZE 1024000000
#define DYNAMIC_MEM_NODE_SIZE sizeof(dynamic_mem_node_t) // 16

typedef struct dynamic_mem_node {
    uint32_t size;
    bool used;
    struct dynamic_mem_node *next;
    struct dynamic_mem_node *prev;
} dynamic_mem_node_t;

void init_dmem();
void *find_memblock(dynamic_mem_node_t *dynamic_mem, size_t size);
void *malloc(size_t size);
void mfree(void *p);
void *merge_next_node_into_current(dynamic_mem_node_t *current_mem_node);
void *merge_current_node_into_previous(dynamic_mem_node_t *current_mem_node);
void *kcalloc(int n, int size);