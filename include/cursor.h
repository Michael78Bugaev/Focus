#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>

// Позиция курсора
extern uint32_t cursor_x;
extern uint32_t cursor_y;

// Инициализация курсора
void cursor_init(void);

// Обновление позиции курсора
void cursor_update(void);

// Перемещение курсора
void cursor_move(uint32_t x, uint32_t y);

// Включение/выключение курсора
void cursor_enable(uint8_t enable);

// Получение состояния курсора
uint8_t cursor_is_visible(void);

#endif 