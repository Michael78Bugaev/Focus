#include <vbe_term.h>
#include <8x8_font.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

// --- VBE framebuffer info ---
static uint32_t fb_addr, fb_pitch, fb_width, fb_height;
static uint8_t fb_bpp;

// --- Terminal buffer ---
static char term_buf[TERM_ROWS][TERM_COLS];
static uint32_t cursor_col = 0, cursor_row = 0;
static int cursor_visible = 1;
static int cursor_blink_counter = 0;

// --- Цвета ---
#define TERM_FG 0xFFFFFFFF
#define TERM_BG 0x00000000
#define TERM_CURSOR 0xFF00FF00

void vbe_term_init(uint32_t addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    fb_addr = addr;
    fb_pitch = pitch;
    fb_width = width;
    fb_height = height;
    fb_bpp = bpp;
    vbe_term_clear();
}

void vbe_term_clear(void) {
    for (size_t r = 0; r < TERM_ROWS; ++r)
        for (size_t c = 0; c < TERM_COLS; ++c)
            term_buf[r][c] = ' ';
    cursor_col = 0;
    cursor_row = 0;
    vbe_term_render();
}

void vbe_term_set_cursor(uint32_t col, uint32_t row) {
    if (col < TERM_COLS && row < TERM_ROWS) {
        cursor_col = col;
        cursor_row = row;
    }
}

void vbe_term_get_cursor(uint32_t* col, uint32_t* row) {
    if (col) *col = cursor_col;
    if (row) *row = cursor_row;
}

static void term_scroll(void) {
    for (size_t r = 1; r < TERM_ROWS; ++r)
        for (size_t c = 0; c < TERM_COLS; ++c)
            term_buf[r-1][c] = term_buf[r][c];
    for (size_t c = 0; c < TERM_COLS; ++c)
        term_buf[TERM_ROWS-1][c] = ' ';
    if (cursor_row > 0) cursor_row--;
}

void vbe_term_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        if (++cursor_row >= TERM_ROWS) {
            term_scroll();
            cursor_row = TERM_ROWS-1;
        }
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) cursor_col--;
        term_buf[cursor_row][cursor_col] = ' ';
    } else {
        term_buf[cursor_row][cursor_col] = c;
        if (++cursor_col >= TERM_COLS) {
            cursor_col = 0;
            if (++cursor_row >= TERM_ROWS) {
                term_scroll();
                cursor_row = TERM_ROWS-1;
            }
        }
    }
}

void vbe_term_puts(const char* str) {
    while (*str) vbe_term_putc(*str++);
}

// --- Курсор: мигание ---
void vbe_term_update_cursor_blink(void) {
    // Вызывать из таймера (например, 20-30 раз в секунду)
    if (++cursor_blink_counter >= 15) { // ~0.5 сек при 30 Гц
        cursor_visible = !cursor_visible;
        cursor_blink_counter = 0;
        vbe_term_render();
    }
}

// --- Рендеринг терминала ---
static void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < fb_width && y < fb_height) {
        uint32_t* pixel = (uint32_t*)(fb_addr + y * fb_pitch + x * 4);
        *pixel = color;
    }
}

void vbe_term_render(void) {
    for (uint32_t row = 0; row < TERM_ROWS; ++row) {
        for (uint32_t col = 0; col < TERM_COLS; ++col) {
            char ch = term_buf[row][col];
            for (uint32_t fy = 0; fy < 8; ++fy) {
                uint8_t bits = font8x8_basic[(unsigned char)ch][fy];
                for (uint32_t fx = 0; fx < 8; ++fx) {
                    uint32_t color = (bits & (1 << fx)) ? TERM_FG : TERM_BG;
                    // Курсор: инвертировать цвет
                    if (cursor_visible && row == cursor_row && col == cursor_col)
                        color = (bits & (1 << fx)) ? TERM_BG : TERM_CURSOR;
                    put_pixel(col*8 + fx, row*8 + fy, color);
                }
            }
        }
    }
}
