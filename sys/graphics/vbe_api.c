#include <vbe_terminal.h>
#include <stdint.h>
#include <string.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

// Bresenham's line algorithm (integer, no floating point)
void vapi_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -((y2 > y1) ? (y2 - y1) : (y1 - y2));
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy; // error value e_xy

    while (1) {
        vbe_pixel((uint32_t)x1, (uint32_t)y1, (uint8_t)color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { // e_xy + e_x > 0
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) { // e_xy + e_y < 0
            err += dx;
            y1 += sy;
        }
    }
}

// Filled rectangle
void vapi_draw_rect(int x, int y, int width, int height, uint32_t color) {
    if (width <= 0 || height <= 0) return;
    for (int yy = y; yy < y + height; yy++) {
        for (int xx = x; xx < x + width; xx++) {
            vbe_pixel((uint32_t)xx, (uint32_t)yy, (uint8_t)color);
        }
    }
}

// Mid-point circle algorithm (8-way symmetry)
void vapi_draw_circle(int x0, int y0, int radius, uint32_t color) {
    if (radius <= 0) return;
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    while (y >= x) {
        // Draw the eight symmetric points
        vbe_pixel(x0 + x, y0 + y, (uint8_t)color);
        vbe_pixel(x0 - x, y0 + y, (uint8_t)color);
        vbe_pixel(x0 + x, y0 - y, (uint8_t)color);
        vbe_pixel(x0 - x, y0 - y, (uint8_t)color);
        vbe_pixel(x0 + y, y0 + x, (uint8_t)color);
        vbe_pixel(x0 - y, y0 + x, (uint8_t)color);
        vbe_pixel(x0 + y, y0 - x, (uint8_t)color);
        vbe_pixel(x0 - y, y0 - x, (uint8_t)color);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

// Draw ASCII text at pixel-precision position (top-left corner)
void vapi_draw_text(int px, int py, const char *text, uint32_t color) {
    if (!text) return;

    // Translate pixel coordinates to character cell coordinates
    int col = px / FONT_WIDTH;
    int row = py / FONT_HEIGHT;

    set_cursor_x((uint16_t)col);
    set_cursor_y((uint16_t)row);

    uint8_t attr = (uint8_t)(color & 0x0F); // foreground, black background
    while (*text) {
        kputchar((uint8_t)(*text++), attr);
    }
}