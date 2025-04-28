#include "vbe.h"
#include <string.h> // for memcpy
#include <stdint.h>
#include "8x8_font.h"

static vbe_mode_info_t vbe_info;

void vbe_init(uint32_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    vbe_info.fb_addr = fb_addr;
    vbe_info.pitch = pitch;
    vbe_info.width = width;
    vbe_info.height = height;
    vbe_info.bpp = bpp;
    size_t bufsize = pitch * height;
    vbe_info.backbuffer = (uint8_t*)0x200000; // Просто адрес в RAM, не пересекающийся с ядром и видеопамятью
    memset(vbe_info.backbuffer, 0, bufsize);
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= vbe_info.width || y >= vbe_info.height) return;
    uint32_t* pixel = (uint32_t*)(vbe_info.backbuffer + y * vbe_info.pitch + x * 4);
    *pixel = color;
}

// Простейший вывод символа: квадрат 8x16
void vbe_draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    unsigned char uc = (unsigned char)c;
    for (uint32_t row = 0; row < 8; ++row) {
        uint8_t bits = font8x8_basic[uc][row];
        for (uint32_t col = 0; col < 8; ++col) {
            if (bits & (1 << col)) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void vbe_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    while (*str) {
        vbe_draw_char(x, y, *str, color);
        x += 8;
        ++str;
    }
}

void vbe_swap_buffers(void) {
    size_t bufsize = vbe_info.pitch * vbe_info.height;
    memcpy((void*)vbe_info.fb_addr, vbe_info.backbuffer, bufsize);
}
