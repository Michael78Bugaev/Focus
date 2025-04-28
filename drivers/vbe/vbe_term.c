#include <vbe_term.h>
#include <8x8_font.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <cursor.h>
#include <vbe.h>

// --- VBE framebuffer info ---
static uint32_t fb_addr, fb_pitch, fb_width, fb_height;
static uint8_t fb_bpp;

// --- Terminal buffer ---
static char term_buf[TERM_ROWS][TERM_COLS];
static uint32_t cursor_col = 0, cursor_row = 0;

// --- Цвета ---
#define TERM_FG 0xFFFFFFFF
#define TERM_BG 0x00000000

// --- Состояние курсора ---
static uint8_t cursor_visible = 1;

// --- Оптимизация рендеринга ---
static uint32_t last_rendered_row = 0;
static uint32_t last_rendered_col = 0;

// --- VBE info ---
extern vbe_mode_info_t vbe_info;

static void draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    unsigned char uc = (unsigned char)c;
    uint32_t* buffer = (uint32_t*)vbe_info.backbuffer;
    uint32_t pitch = vbe_info.pitch / 4;
    
    for (uint32_t row = 0; row < 8; ++row) {
        uint8_t bits = font8x8_basic[uc][row];
        uint32_t offset = (y + row) * pitch + x;
        
        for (uint32_t col = 0; col < 8; ++col) {
            if (bits & (1 << col)) {
                buffer[offset + col] = color;
            } else {
                buffer[offset + col] = TERM_BG;
            }
        }
    }
}

static void term_scroll(void) {
    // Сдвигаем содержимое буфера вверх
    for (size_t r = 0; r < TERM_ROWS - 1; ++r) {
        for (size_t c = 0; c < TERM_COLS; ++c) {
            term_buf[r][c] = term_buf[r + 1][c];
        }
    }
    
    // Очищаем последнюю строку
    for (size_t c = 0; c < TERM_COLS; ++c) {
        term_buf[TERM_ROWS-1][c] = ' ';
    }
    
    // Очищаем экран и перерисовываем
    vbe_clear_buffer(TERM_BG);
    for (size_t r = 0; r < TERM_ROWS; ++r) {
        for (size_t c = 0; c < TERM_COLS; ++c) {
            draw_char(c * 8, r * 8, term_buf[r][c], TERM_FG);
        }
    }
}

void vbe_term_init(uint32_t addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    fb_addr = addr;
    fb_pitch = pitch;
    fb_width = width;
    fb_height = height;
    fb_bpp = bpp;
    vbe_term_clear();
}

void vbe_term_clear(void) {
    for (size_t r = 0; r < TERM_ROWS; ++r) {
        for (size_t c = 0; c < TERM_COLS; ++c) {
            term_buf[r][c] = ' ';
        }
    }
    cursor_col = 0;
    cursor_row = 0;
    cursor_move(0, 0);
    vbe_clear_buffer(TERM_BG);
    vbe_swap_buffers();
}

void vbe_term_set_cursor(uint32_t col, uint32_t row) {
    if (col < TERM_COLS && row < TERM_ROWS) {
        cursor_col = col;
        cursor_row = row;
        cursor_move(col, row);
    }
}

void vbe_term_get_cursor(uint32_t* col, uint32_t* row) {
    if (col) *col = cursor_col;
    if (row) *row = cursor_row;
}

void vbe_term_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        if (cursor_row < TERM_ROWS - 1) {
            cursor_row++;
        } else {
            term_scroll();
        }
        cursor_move(cursor_col, cursor_row);
        return;
    }

    if (c == '\r') {
        cursor_col = 0;
        cursor_move(cursor_col, cursor_row);
        return;
    }

    if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= TERM_COLS) {
            cursor_col = 0;
            if (cursor_row < TERM_ROWS - 1) {
                cursor_row++;
            } else {
                term_scroll();
            }
        }
        cursor_move(cursor_col, cursor_row);
        return;
    }

    // Обычный символ
    term_buf[cursor_row][cursor_col] = c;
    draw_char(cursor_col * 8, cursor_row * 8, c, TERM_FG);
    cursor_col++;

    if (cursor_col >= TERM_COLS) {
        cursor_col = 0;
        if (cursor_row < TERM_ROWS - 1) {
            cursor_row++;
        } else {
            term_scroll();
        }
    }

    cursor_move(cursor_col, cursor_row);
}

void vbe_term_puts(const char* str) {
    while (*str) {
        vbe_term_putc(*str++);
    }
}

void vbe_term_update_cursor_blink(void) {
    if (cursor_visible) {
        // Рисуем курсор
        draw_char(cursor_col * 8, cursor_row * 8, '_', TERM_FG);
    }
}

void vbe_term_render(void) {
    // Копируем только измененную область
    uint32_t* src = (uint32_t*)vbe_info.backbuffer;
    uint32_t* dst = (uint32_t*)vbe_info.fb_addr;
    uint32_t pitch = vbe_info.pitch / 4;
    
    // Копируем только последнюю измененную строку
    uint32_t row_offset = last_rendered_row * pitch;
    for (size_t i = 0; i < TERM_COLS * 8; ++i) {
        dst[row_offset + i] = src[row_offset + i];
    }
    
    // Обновляем позицию последнего рендера
    last_rendered_row = cursor_row;
    last_rendered_col = cursor_col;
}
