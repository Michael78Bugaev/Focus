#ifndef VBE_H
#define VBE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t fb_addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t* backbuffer;
} vbe_mode_info_t;

void vbe_init(uint32_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp);
void vbe_draw_char(uint32_t x, uint32_t y, char c, uint32_t color);
void vbe_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color);
void vbe_swap_buffers(void);

#endif