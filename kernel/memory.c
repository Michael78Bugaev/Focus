#include <stdint.h>
#include <stddef.h>

#define MEMORY_SIZE 0x1000000 // 16MB
#define BLOCK_SIZE 4096

static uint8_t memory_pool[MEMORY_SIZE];
static uint8_t memory_map[MEMORY_SIZE / BLOCK_SIZE];

void* malloc(size_t size) {
    size_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (size_t i = 0; i < sizeof(memory_map); i++) {
        if (memory_map[i] == 0) {
            size_t j;
            for (j = 0; j < blocks_needed; j++) {
                if (i + j >= sizeof(memory_map) || memory_map[i + j] != 0) {
                    break;
                }
            }
            if (j == blocks_needed) {
                for (j = 0; j < blocks_needed; j++) {
                    memory_map[i + j] = 1;
                }
                return &memory_pool[i * BLOCK_SIZE];
            }
        }
    }
    return NULL;
}

void free(void* ptr) {
    if (ptr == NULL) return;
    
    size_t block_index = ((uint8_t*)ptr - memory_pool) / BLOCK_SIZE;
    size_t i = block_index;
    
    while (i < sizeof(memory_map) && memory_map[i] == 1) {
        memory_map[i] = 0;
        i++;
    }
} 