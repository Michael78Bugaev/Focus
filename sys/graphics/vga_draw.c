#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((uint16_t*)0xB8000)

// Цвета VGA (4 бита на фон, 4 бита на символ)
// color: 0xXY, X - фон, Y - символ

void api_draw_rectangle(int width, int height, int x, int y, int color) {
    for (int dy = 0; dy < height; dy++) {
        int row = y + dy;
        if (row < 0 || row >= VGA_HEIGHT) continue;
        for (int dx = 0; dx < width; dx++) {
            int col = x + dx;
            if (col < 0 || col >= VGA_WIDTH) continue;
            int offset = row * VGA_WIDTH + col;
            VGA_MEM[offset] = (color << 8) | ' ';
        }
    }
}

void api_draw_hline(int x, int y, int length, int color) {
    api_draw_rectangle(length, 1, x, y, color);
}

void api_draw_vline(int x, int y, int length, int color) {
    api_draw_rectangle(1, length, x, y, color);
}

void api_draw_text(int x, int y, const char* text, int color) {
    if (y < 0 || y >= VGA_HEIGHT) return;
    int col = x;
    int offset = y * VGA_WIDTH + col;
    for (int i = 0; text[i] && col < VGA_WIDTH; i++, col++) {
        VGA_MEM[offset + i] = (color << 8) | text[i];
    }
}

// Можно добавить другие примитивы по аналогии 

void save_vga_screen(void* buffer) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        ((uint16_t*)buffer)[i] = VGA_MEM[i];
    }
}

void restore_vga_screen(void* buffer) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEM[i] = ((uint16_t*)buffer)[i];
    }
}

int *save_cursor_pos(int x, int y) {
    int *pos = {x, y};
    return pos;
}