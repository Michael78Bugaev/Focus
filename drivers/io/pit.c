#include <stdint.h>
#include <pit.h>
#include <io.h>
#include <idt.h>
#include <vbe_term.h>
#include <isr.h>

// Глобальные переменные для отслеживания времени
static uint64_t ticks = 0;
static const uint32_t freq = 1000; // 1000 Гц
static const unsigned int blink_interval = 500; // Интервал мигания в миллисекундах
static unsigned int last_blink_time = 0; // Время последнего мигания

// Обработчик прерывания PIT
static void pit_handler(void) {
    ticks++;

    // Проверяем, нужно ли мигать курсором
    if (ticks * (1000 / freq) - last_blink_time >= blink_interval) {
        vbe_term_update_cursor_blink();
        last_blink_time = ticks * (1000 / freq);
    }

    // Подтверждаем прерывание
    outb(0x20, 0x20);
}

// Инициализация PIT
void pit_init(uint32_t frequency) {
    // Устанавливаем частоту
    uint32_t divisor = 1193180 / frequency;

    // Отправляем команду в PIT
    outb(0x43, 0x36); // Командный байт: канал 0, режим 3, двоичный счетчик

    // Отправляем делитель частоты
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    // Регистрируем обработчик прерывания
    register_interrupt_handler(32, pit_handler);
}

// Получение текущего количества тиков
uint64_t pit_get_ticks(void) {
    return ticks;
}

// Установка количества тиков
void pit_set_ticks(uint64_t new_ticks) {
    ticks = new_ticks;
}