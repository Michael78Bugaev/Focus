#include "vbe.h"
#include <string.h> // for memcpy
#include <stdint.h>
#include "8x8_font.h"
#include "memory.h"

vbe_mode_info_t vbe_info;

void vbe_init(uint32_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    vbe_info.fb_addr = fb_addr;
    vbe_info.pitch = pitch;
    vbe_info.width = width;
    vbe_info.height = height;
    vbe_info.bpp = bpp;

    // Выделяем память для буферов
    size_t buffer_size = pitch * height;
    vbe_info.backbuffer = (uint8_t*)malloc(buffer_size);
    vbe_info.frontbuffer = (uint8_t*)malloc(buffer_size);

    // Инициализируем буферы черным цветом
    vbe_clear_buffer(0x00000000);
    vbe_copy_buffer();
}

void vbe_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= vbe_info.width || y >= vbe_info.height) return;

    // Получаем указатель на пиксель в заднем буфере
    uint32_t* buffer = (uint32_t*)vbe_info.backbuffer;
    uint32_t offset = y * (vbe_info.pitch / 4) + x;
    buffer[offset] = color;
}

void vbe_clear_buffer(uint32_t color) {
    size_t buffer_size = vbe_info.pitch * vbe_info.height;
    uint32_t* buffer = (uint32_t*)vbe_info.backbuffer;
    size_t pixel_count = buffer_size / (vbe_info.bpp / 8);

    for (size_t i = 0; i < pixel_count; i++) {
        buffer[i] = color;
    }
}

void vbe_copy_buffer(void) {
    size_t buffer_size = vbe_info.pitch * vbe_info.height;
    memcpy(vbe_info.frontbuffer, vbe_info.backbuffer, buffer_size);
}

void vbe_swap_buffers(void) {
    // Копируем содержимое заднего буфера в видеопамять
    size_t buffer_size = vbe_info.pitch * vbe_info.height;
    memcpy((void*)vbe_info.fb_addr, vbe_info.backbuffer, buffer_size);
}

void vbe_draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    if (x >= vbe_info.width || y >= vbe_info.height) return;

    // Получаем указатель на пиксель в заднем буфере
    uint32_t* buffer = (uint32_t*)vbe_info.backbuffer;
    uint32_t offset = y * (vbe_info.pitch / 4) + x;
    buffer[offset] = color;
}

void vbe_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    while (*str) {
        vbe_draw_char(x, y, *str, color);
        x++;
        str++;
    }
}
