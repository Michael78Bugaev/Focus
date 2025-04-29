#include <ata.h>
#include <ports.h>
#include <kernel.h>
#include <string.h>

// Глобальные переменные
ata_device_t ata_devices[MAX_ATA_DEVICES];
int ata_device_count = 0;
uint16_t ata_buffer[256];

// Ожидание освобождения устройства
int ata_wait_busy(uint16_t base) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(base + 7) & ATA_SR_BSY))
            return 1;
    }
    return 0;
}

// Ожидание готовности данных
int ata_wait_drq(uint16_t base) {
    for (int i = 0; i < 100000; i++) {
        if (inb(base + 7) & ATA_SR_DRQ)
            return 1;
    }
    return 0;
}

// Чтение статуса
uint8_t ata_read_status(uint16_t base) {
    return inb(base + 7);
}

// Запись команды
void ata_write_command(uint16_t base, uint8_t command) {
    outb(base + 7, command);
}

// Запись LBA адреса
void ata_write_lba(uint16_t base, uint32_t lba, uint8_t drive) {
    outb(base + 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(base + 2, 1); // Количество секторов
    outb(base + 3, lba & 0xFF);
    outb(base + 4, (lba >> 8) & 0xFF);
    outb(base + 5, (lba >> 16) & 0xFF);
}

// Преобразование строки модели из IDENTIFY данных
void ata_string_swap(char *str, int len) {
    for (int i = 0; i < len; i += 2) {
        char tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }
    
    // Обрезаем пробелы в конце
    for (int i = len - 1; i >= 0; i--) {
        if (str[i] == ' ') {
            str[i] = 0;
        } else {
            break;
        }
    }
}

// Идентификация устройства
int ata_identify(uint8_t bus, uint8_t drive) {
    uint16_t base = bus ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
    
    // Выбор устройства
    outb(base + 6, 0xA0 | (drive << 4));
    
    // Ожидание готовности
    if (!ata_wait_busy(base)) return 0;
    
    // Отправка команды IDENTIFY
    outb(base + 7, ATA_CMD_IDENTIFY);
    
    // Проверка наличия устройства
    uint8_t status = inb(base + 7);
    if (status == 0 || status == 0x7F || status == 0xFF) return 0;
    
    if (!ata_wait_busy(base)) return 0;
    if (!ata_wait_drq(base)) return 0;
    
    // Чтение данных
    for (int i = 0; i < 256; i++) {
        ata_buffer[i] = inw(base);
    }
    
    // Копирование и преобразование информации об устройстве
    // Модель находится в словах 27-46 (40 байт)
    char *model = ata_devices[ata_device_count].model;
    for (int i = 0; i < 20; i++) {
        uint16_t word = ata_buffer[27 + i];
        // Меняем порядок байт в каждом слове
        model[i*2] = (word >> 8) & 0xFF;
        model[i*2 + 1] = word & 0xFF;
    }
    model[40] = 0;
    
    // Удаляем пробелы в конце строки
    for (int i = 39; i >= 0; i--) {
        if (model[i] == ' ') {
            model[i] = 0;
        } else if (model[i] != 0) {
            break;
        }
    }
    
    // Определение размера устройства
    uint32_t sectors = *(uint32_t*)&ata_buffer[60];
    ata_devices[ata_device_count].size = sectors * 512;
    
    return 1;
}

// Инициализация ATA драйвера
void ata_init() {
    ata_device_count = 0;
    
    // Проверка первичного канала
    if (ata_identify(0, ATA_MASTER)) {
        ata_devices[ata_device_count].bus = 0;
        ata_devices[ata_device_count].drive = ATA_MASTER;
        ata_devices[ata_device_count].type = DEVICE_TYPE_ATA;
        ata_device_count++;
    }
    
    if (ata_identify(0, ATA_SLAVE)) {
        ata_devices[ata_device_count].bus = 0;
        ata_devices[ata_device_count].drive = ATA_SLAVE;
        ata_devices[ata_device_count].type = DEVICE_TYPE_ATA;
        ata_device_count++;
    }
    
    // Проверка вторичного канала
    if (ata_identify(1, ATA_MASTER)) {
        ata_devices[ata_device_count].bus = 1;
        ata_devices[ata_device_count].drive = ATA_MASTER;
        ata_devices[ata_device_count].type = DEVICE_TYPE_ATA;
        ata_device_count++;
    }
    
    if (ata_identify(1, ATA_SLAVE)) {
        ata_devices[ata_device_count].bus = 1;
        ata_devices[ata_device_count].drive = ATA_SLAVE;
        ata_devices[ata_device_count].type = DEVICE_TYPE_ATA;
        ata_device_count++;
    }
    
    // Вывод информации о найденных устройствах
    if (ata_device_count > 0) {
        kprintf("Found %d ATA device(s):\n", ata_device_count);
        for (int i = 0; i < ata_device_count; i++) {
            kprintf("%d: %s (%d MB)\n", 
                i, 
                ata_devices[i].model,
                ata_devices[i].size / (1024 * 1024)
            );
        }
    }
}

// Чтение секторов
int ata_read_sectors(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer) {
    if (device_id >= ata_device_count) return 0;
    
    uint16_t base = ata_devices[device_id].bus ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
    uint8_t drive = ata_devices[device_id].drive;
    
    if (!ata_wait_busy(base)) return 0;
    
    ata_write_lba(base, lba, drive);
    outb(base + 7, ATA_CMD_READ_PIO);
    
    for (int sector = 0; sector < num_sectors; sector++) {
        if (!ata_wait_busy(base)) return 0;
        if (!ata_wait_drq(base)) return 0;
        
        for (int i = 0; i < 256; i++) {
            buffer[sector * 256 + i] = inw(base);
        }
    }
    
    return 1;
}

// Запись секторов
int ata_write_sectors(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer) {
    if (device_id >= ata_device_count) return 0;
    
    uint16_t base = ata_devices[device_id].bus ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
    uint8_t drive = ata_devices[device_id].drive;
    
    if (!ata_wait_busy(base)) return 0;
    
    ata_write_lba(base, lba, drive);
    outb(base + 7, ATA_CMD_WRITE_PIO);
    
    for (int sector = 0; sector < num_sectors; sector++) {
        if (!ata_wait_busy(base)) return 0;
        if (!ata_wait_drq(base)) return 0;
        
        for (int i = 0; i < 256; i++) {
            outw(base, buffer[sector * 256 + i]);
        }
    }
    
    // Сброс кэша
    outb(base + 7, ATA_CMD_CACHE_FLUSH);
    if (!ata_wait_busy(base)) return 0;
    
    return 1;
}

// Чтение ATAPI устройства
int atapi_read(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer) {
    if (device_id >= ata_device_count || ata_devices[device_id].type != DEVICE_TYPE_ATAPI) {
        return 0;
    }
    
    uint16_t base = ata_devices[device_id].bus ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
    uint8_t drive = ata_devices[device_id].drive;
    
    if (!ata_wait_busy(base)) return 0;
    
    ata_write_lba(base, lba, drive);
    outb(base + 7, ATA_CMD_PACKET);
    
    // Отправка команды READ (12)
    uint8_t packet[12] = {
        0xA8, 0x00, 0x00, 0x00,
        (lba >> 24) & 0xFF,
        (lba >> 16) & 0xFF,
        (lba >> 8) & 0xFF,
        lba & 0xFF,
        num_sectors,
        0x00, 0x00, 0x00
    };
    
    for (int i = 0; i < 6; i++) {
        outw(base, packet[i * 2] | (packet[i * 2 + 1] << 8));
    }
    
    for (int sector = 0; sector < num_sectors; sector++) {
        if (!ata_wait_busy(base)) return 0;
        if (!ata_wait_drq(base)) return 0;
        
        for (int i = 0; i < 256; i++) {
            buffer[sector * 256 + i] = inw(base);
        }
    }
    
    return 1;
}

// Получение количества найденных устройств
int ata_get_device_count() {
    return ata_device_count;
}

// Получение имени устройства
const char* ata_get_device_name(uint8_t device_id) {
    if (device_id >= ata_device_count) {
        return "Unknown";
    }
    return ata_devices[device_id].model;
}

// Получение размера устройства
uint32_t ata_get_device_size(uint8_t device_id) {
    if (device_id >= ata_device_count) {
        return 0;
    }
    return ata_devices[device_id].size;
} 