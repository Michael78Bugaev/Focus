#include <stdint.h>
#include <vbe_term.h>
#include <cursor.h>

void terminal_init(void) {
    cursor_init();
    vbe_term_clear();
}

void terminal_loop(void) {
    uint32_t counter = 0;
    while (1) {
        // Обновляем курсор
        cursor_update();
        
        // Выводим текст
        vbe_term_puts("Hello, World! Counter: ");
        char num_str[32];
        uint32_t num = counter++;
        uint32_t i = 0;
        do {
            num_str[i++] = '0' + (num % 10);
            num /= 10;
        } while (num > 0);
        num_str[i] = '\0';
        
        // Переворачиваем строку с числом
        for (uint32_t j = 0; j < i/2; j++) {
            char temp = num_str[j];
            num_str[j] = num_str[i-j-1];
            num_str[i-j-1] = temp;
        }
        
        vbe_term_puts(num_str);
        vbe_term_putc('\n');
        
        // Обновляем экран
        vbe_term_render();
    }
} 