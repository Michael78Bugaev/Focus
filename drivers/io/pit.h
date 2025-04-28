#ifndef PIT_H
#define PIT_H

#include <stdint.h>

// Инициализация PIT с указанной частотой
void pit_init(uint32_t frequency);

// Получение текущего количества тиков
uint64_t pit_get_ticks(void);

// Установка количества тиков
void pit_set_ticks(uint64_t new_ticks);

#endif // PIT_H 