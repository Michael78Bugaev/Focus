#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// Функция получения даты и времени из BIOS
void get_date_time(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint8_t *year) {
    // Читаем регистры CMOS
    *second = read_cmos(0x00);
    *minute = read_cmos(0x02);
    *hour = read_cmos(0x04);
    *day = read_cmos(0x07);
    *month = read_cmos(0x08);
    *year = read_cmos(0x09);
}