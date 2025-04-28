#include <stdint.h>
#include <cursor.h>
#include <vbe.h>

// Позиция курсора
uint32_t cursor_x = 0;
uint32_t cursor_y = 0;

// Состояние курсора
static uint8_t cursor_visible = 1;

// Цвет курсора (белый)
#define CURSOR_COLOR 0xFFFFFFFF

void cursor_init(void) {
    cursor_visible = 1;
    cursor_x = 0;
    cursor_y = 0;
    cursor_update();
}

void cursor_update(void) {
    if (cursor_visible) {
        // Рисуем курсор как подчеркивание
        for (uint32_t x = 0; x < 8; x++) {
            vbe_draw_pixel(cursor_x * 8 + x, cursor_y * 8 + 7, CURSOR_COLOR);
        }
    }
}

void cursor_move(uint32_t x, uint32_t y) {
    // Стираем старый курсор
    for (uint32_t i = 0; i < 8; i++) {
        vbe_draw_pixel(cursor_x * 8 + i, cursor_y * 8 + 7, 0x00000000);
    }
    
    // Обновляем позицию
    cursor_x = x;
    cursor_y = y;
    
    // Рисуем новый курсор
    cursor_update();
}

void cursor_enable(uint8_t enable) {
    cursor_visible = enable;
    cursor_update();
}

uint8_t cursor_is_visible(void) {
    return cursor_visible;
} 