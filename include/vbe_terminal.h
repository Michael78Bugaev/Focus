#pragma once
#include <stdint.h>
#include <multiboot.h>

// External font provided by user: 8x16 glyphs


// Initialize VBE terminal using multiboot info
void vbe_init(struct multiboot_info *mbi);

// Terminal functions (override VGA text mode)
void kputchar(uint8_t ch, uint8_t attr);
void kclear(void);
void kprint(uint8_t *str);

extern int cursor_blink; 

// Expose VBE framebuffer swap and cursor positioning
void vbe_swap(void);
void set_cursor_x(uint16_t x);
void set_cursor_y(uint16_t y); 

// Expose glyph array and pixel drawing API
//extern const uint8_t font8x16[][16];
void vbe_putpixel(uint32_t x, uint32_t y, uint8_t color); 