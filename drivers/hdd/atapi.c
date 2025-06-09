#include "atapi.h"
#include "ata.h"
#include <ports.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define ATAPI_CMD_PACKET 0xA0
#define ATAPI_CMD_IDENTIFY_PACKET 0xA1
#define ATAPI_CMD_READ_12 0xA8
#define ATAPI_SECTOR_SIZE 2048
#define ATAPI_TIMEOUT 1000000
#define MAX_ATAPI_DEVICES 4

static struct atapi_device g_atapi_devs[MAX_ATAPI_DEVICES] = {
    {0, 0, 0x1F0, 0x3F6, 0, 0, 0, ""},
    {0, 1, 0x1F0, 0x3F6, 1, 0, 0, ""},
    {1, 0, 0x170, 0x376, 0, 0, 0, ""},
    {1, 1, 0x170, 0x376, 1, 0, 0, ""}
};

struct atapi_device* atapi_get_device(int num) {
    if (num < 0 || num >= MAX_ATAPI_DEVICES) return NULL;
    return &g_atapi_devs[num];
}

static int atapi_wait_bsy_drq_timeout(uint16_t io_base) {
    uint8_t status;
    int timeout = ATAPI_TIMEOUT;
    do {
        status = inb(io_base + 7);
        if (--timeout == 0) return -1;
    } while (status & 0x80); // BSY
    timeout = ATAPI_TIMEOUT;
    do {
        status = inb(io_base + 7);
        if (--timeout == 0) return -1;
    } while (!(status & 0x08)); // DRQ
    return 0;
}

void atapi_init() {
    for (int i = 0; i < MAX_ATAPI_DEVICES; i++) {
        struct atapi_device* dev = &g_atapi_devs[i];

        // 1. Сброс контроллера
        outb(dev->control_base, 0x04); // SRST=1
        for (volatile int d = 0; d < 10000; d++);
        outb(dev->control_base, 0x00); // SRST=0
        for (volatile int d = 0; d < 10000; d++);

        // 2. Выбрать устройство
        outb(dev->io_base + 6, 0xA0 | (dev->slavebit << 4));
        for (volatile int d = 0; d < 10000; d++);

        // 3. Прочитать сигнатуру
        uint8_t sc = inb(dev->io_base + 2); // Sector Count
        uint8_t sn = inb(dev->io_base + 3); // Sector Number
        uint8_t cl = inb(dev->io_base + 4); // Cylinder Low
        uint8_t ch = inb(dev->io_base + 5); // Cylinder High

        kprintf("ATAPI #%d: sc=%02X sn=%02X cl=%02X ch=%02X\n", i, sc, sn, cl, ch);

        // ATAPI сигнатура: 0x01 0x01 0x14 0xEB
        if (cl != 0x14 || ch != 0xEB) {
            dev->exists = 0;
            continue;
        }

        // 4. Отправить IDENTIFY PACKET DEVICE (0xA1)
        outb(dev->io_base + 7, ATAPI_CMD_IDENTIFY_PACKET);

        // 5. Ждать DRQ (и не ERR)
        int timeout = ATAPI_TIMEOUT;
        uint8_t status;
        do {
            status = inb(dev->io_base + 7);
            if (status & 0x01) break; // ERR
            if (--timeout == 0) break;
        } while (!(status & 0x08)); // DRQ

        if (timeout == 0 || (status & 0x01)) {
            dev->exists = 0;
            continue;
        }

        // 6. Прочитать 512 байт (256 слов)
        uint16_t buffer[256];
        for (int j = 0; j < 256; j++) buffer[j] = inw(dev->io_base + 0);

        // kprintf для отладки
        kprintf("IDENTIFY: %04X %04X %04X %04X\n", buffer[0], buffer[1], buffer[2], buffer[3]);

        // 7. Если сигнатура совпала — считаем устройство ATAPI!
        dev->exists = 1;
        for (int k = 0; k < 40; k += 2) {
            dev->model[k] = buffer[27 + k / 2] >> 8;
            dev->model[k + 1] = buffer[27 + k / 2] & 0xFF;
        }
        dev->model[40] = 0;
    }

    // Диагностика
    for (int i = 0; i < 4; i++) {
        struct atapi_device* dev = atapi_get_device(i);
        kprintf("ATAPI #%d: exists=%d, model=%s\n", i, dev->exists, dev->model);
    }
}

int atapi_read_device(int devnum, uint32_t lba, uint16_t count, void* buffer) {
    struct atapi_device* dev = atapi_get_device(devnum);
    if (!dev || !dev->exists) {kprintf("Error -1\n"); return -1;}
    uint16_t io = dev->io_base;
    for (uint16_t i = 0; i < count; i++) {
        outb(io + 6, 0xA0 | (dev->slavebit << 4));
        for (volatile int d = 0; d < 10000; d++);

        // Ждать BSY=0
        int timeout = ATAPI_TIMEOUT;
        while ((inb(io + 7) & 0x80) && --timeout > 0);
        if (timeout == 0) {kprintf("Timeout before PACKET\n"); return -4;}

        // ВАЖНО: Установить max byte count (2048 байт)
        outb(io + 1, 0);      // Features
        outb(io + 2, 0);      // Sector Count
        outb(io + 3, 0);      // LBA High
        outb(io + 4, 0x00);   // LBA Low
        outb(io + 5, 0x08);   // LBA Mid = 0x08 (2048 байт)

        outb(io + 7, ATAPI_CMD_PACKET);

        // Ждать DRQ
        timeout = ATAPI_TIMEOUT;
        while (!(inb(io + 7) & 0x08) && --timeout > 0);
        if (timeout == 0) {kprintf("Error -2 (no DRQ after PACKET)\n"); return -2;}

        // READ(12) пакет
        uint8_t packet[12] = {0xA8, 0, (lba >> 24) & 0xFF, (lba >> 16) & 0xFF, (lba >> 8) & 0xFF, lba & 0xFF, 0, 0, 0, 1, 0, 0};
        outsw(io, packet, 6);

        // Ждать DRQ для данных
        timeout = ATAPI_TIMEOUT;
        while (!(inb(io + 7) & 0x08) && --timeout > 0);
        if (timeout == 0) {kprintf("Error -3 (no DRQ for data)\n"); return -3;}

        // Чтение данных
        insw(io, (uint8_t*)buffer + i * ATAPI_SECTOR_SIZE, ATAPI_SECTOR_SIZE / 2);

        // Ждать завершения (BSY=0)
        timeout = ATAPI_TIMEOUT;
        while ((inb(io + 7) & 0x80) && --timeout > 0);
        if (timeout == 0) {kprintf("Timeout after read\n"); return -4;}
    }
    return 0;
}

// Универсальная PACKET-команда (например, для будущих расширений)
int atapi_send_packet(struct atapi_device* dev, uint8_t* packet, void* buffer, uint16_t size) {
    uint16_t io = dev->io_base;
    outb(io + 6, 0xA0 | (dev->slavebit << 4));
    outb(io + 1, 0); outb(io + 4, 0); outb(io + 5, 0); outb(io + 2, 0); outb(io + 3, 0);
    outb(io + 7, ATAPI_CMD_PACKET);
    if (atapi_wait_bsy_drq_timeout(io) < 0) return -2;
    outsw(io, packet, 6);
    if (atapi_wait_bsy_drq_timeout(io) < 0) return -3;
    insw(io, buffer, size / 2);
    int timeout = ATAPI_TIMEOUT;
    while ((inb(io + 7) & 0x80) && --timeout > 0);
    if (timeout == 0) return -4;
    return 0;
}

int atapi_identify(struct atapi_device* dev) {
    uint16_t io = dev->io_base;
    outb(io + 6, 0xA0 | (dev->slavebit << 4));
    outb(io + 7, ATAPI_CMD_IDENTIFY_PACKET);
    int timeout = ATAPI_TIMEOUT;
    uint8_t status;
    do {
        status = inb(io + 7);
        if (--timeout == 0) return -1;
    } while (!(status & 0x08));
    uint16_t buffer[256];
    for (int i = 0; i < 256; i++) buffer[i] = inw(io + 0);
    for (int i = 0; i < 40; i += 2) {
        dev->model[i] = buffer[27 + i / 2] >> 8;
        dev->model[i + 1] = buffer[27 + i / 2] & 0xFF;
    }
    dev->model[40] = 0;
    return 0;
}