#pragma once
#include <stdint.h>
#include <multiboot.h>

// External font provided by user: 8x16 glyphs
extern uint8_t font8x16[256][16];

// Initialize VBE terminal using multiboot info
void vbe_init(struct multiboot_info *mbi);

// Terminal functions (override VGA text mode)
void kputchar(uint8_t ch, uint8_t attr);
void kclear(void);
void kprint(uint8_t *str);

extern int cursor_blink; 