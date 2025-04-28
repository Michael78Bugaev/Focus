#ifndef VBE_TERM_H
#define VBE_TERM_H

#include <stdint.h>
#include <stddef.h>

#define TERM_COLS 100
#define TERM_ROWS 75

void vbe_term_init(uint32_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp);
void vbe_term_clear(void);
void vbe_term_putc(char c);
void vbe_term_puts(const char* str);
void vbe_term_set_cursor(uint32_t col, uint32_t row);
void vbe_term_get_cursor(uint32_t* col, uint32_t* row);
void vbe_term_update_cursor_blink(void); // вызывать в таймере
void vbe_term_render(void); // копирует буфер в видеопамять

#endif
