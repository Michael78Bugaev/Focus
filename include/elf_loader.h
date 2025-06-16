#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load ELF image from memory buffer, set up user stack, return entry address executed.
 * user_stack_top – address where ESP will be set (exclusive top). */
int elf_load_image(const uint8_t *file, uint32_t size, uint32_t user_stack_top,
                   void (**entry_out)(void));

#ifdef __cplusplus
}
#endif 