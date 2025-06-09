#include <ata.h>
#include <ports.h>
#include <string.h>

// Массив для хранения информации о дисках
static ata_drive_t drives[4]; // 4 возможных диска (primary master/slave, secondary master/slave)

// Функция ожидания готовности диска
static void ata_wait(uint16_t base) {
    uint8_t status;
    do {
        status = inb(base + ATA_STATUS);
    } while (status & ATA_SR_BSY);
}

// Функция проверки ошибок
static int ata_check_error(uint16_t base) {
    uint8_t status = inb(base + ATA_STATUS);
    if (status & ATA_SR_ERR) return -1;
    return 0;
}

// Функция выбора диска
static void ata_select_drive(uint16_t base, uint8_t drive) {
    uint8_t value = 0xE0 | (drive << 4); // 0xE0 для LBA режима
    outb(base + ATA_DRIVE, value);
    ata_wait(base);
}

// Функция инициализации диска
static int ata_init_drive(uint16_t base, uint8_t drive) {
    ata_select_drive(base, drive);
    
    // Отправляем команду IDENTIFY
    outb(base + ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_wait(base);
    
    if (ata_check_error(base)) return -1;
    
    // Читаем ответ
    uint16_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base + ATA_DATA);
    }
    
    // Заполняем информацию о диске
    drives[drive].present = 1;
    drives[drive].type = 1; // ATA
    drives[drive].sectors = *(uint32_t*)&buffer[60];
    drives[drive].size = drives[drive].sectors * 512;
    
    // Копируем имя диска
    char* name = (char*)&buffer[27];
    for (int i = 0; i < 20; i++) {
        drives[drive].name[i*2] = name[i*2+1];
        drives[drive].name[i*2+1] = name[i*2];
    }
    drives[drive].name[40] = '\0';
    
    return 0;
}

// Функция для чтения идентификационных данных устройства
static void read_device_info(uint16_t base, char* model) {
    // Отправляем команду IDENTIFY
    outb(base + ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_wait(base);
    
    if (ata_check_error(base)) {
        strcpy(model, "Unknown");
        return;
    }
    
    // Читаем ответ
    uint16_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base + ATA_DATA);
    }
    
    // Копируем имя модели (первые 20 слов)
    char* name = (char*)&buffer[27];
    for (int i = 0; i < 20; i++) {
        model[i*2] = name[i*2+1];
        model[i*2+1] = name[i*2];
    }
    model[40] = '\0';
    
    // Удаляем пробелы в конце
    int len = strlen(model);
    while (len > 0 && model[len-1] == ' ') {
        model[len-1] = '\0';
        len--;
    }
}

// Функция для идентификации ATA-устройства и чтения модели
static int ata_identify_device(uint16_t base, uint8_t drive, char* model) {
    // Сбросить контроллер
    outb(base + ATA_DRIVE, 0xA0 | (drive << 4));
    inb(base + ATA_STATUS);
    // Ждать BSY=0
    for (int i = 0; i < 1000; i++) {
        if (!(inb(base + ATA_STATUS) & 0x80)) break;
    }
    // Отправить IDENTIFY
    outb(base + ATA_COMMAND, 0xEC);
    // Ждать BSY=0 и DRQ=1
    uint8_t status = 0;
    for (int i = 0; i < 1000; i++) {
        status = inb(base + ATA_STATUS);
        if (!(status & 0x80) && (status & 0x08)) break;
    }
    if (!(status & 0x08)) return -1; // Нет DRQ — нет устройства

    // Прочитать 256 слов
    uint16_t buffer[256];
    for (int i = 0; i < 256; i++) buffer[i] = inw(base + ATA_DATA);

    // Копировать модель (40 байт, слова 27-46)
    for (int i = 0; i < 20; i++) {
        model[i*2] = buffer[27+i] >> 8;
        model[i*2+1] = buffer[27+i] & 0xFF;
    }
    model[40] = 0;
    // Удалить пробелы в конце
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;
    return 0;
}

// Инициализация ATA контроллера
void ata_init() {
    memset(drives, 0, sizeof(drives));
    
    // Проверяем primary master
    if (ata_init_drive(ATA_PRIMARY_BASE, 0) == 0) {
        drives[0].present = 1;
        kprintf("Found ATA primary master: %s\n", drives[0].name);
    }
    
    // Проверяем primary slave
    if (ata_init_drive(ATA_PRIMARY_BASE, 1) == 0) {
        drives[1].present = 1;
        kprintf("Found ATA primary slave: %s\n", drives[1].name);
    }
    
    // Проверяем secondary master
    if (ata_init_drive(ATA_SECONDARY_BASE, 0) == 0) {
        drives[2].present = 1;
        kprintf("Found ATA secondary master: %s\n", drives[2].name);
    }
    
    // Проверяем secondary slave
    if (ata_init_drive(ATA_SECONDARY_BASE, 1) == 0) {
        drives[3].present = 1;
        kprintf("Found ATA secondary slave: %s\n", drives[3].name);
    }

    // Остальные устройства — только наличие контроллера
    if (inb(0x1F7) != 0xFF) {
        kprint("Found IDE controller at 0x1F0\n");
    }
    if (inb(0x177) != 0xFF) {
        kprint("Found SATA controller at 0x170\n");
    }
    if (inb(0x337) != 0xFF) {
        kprint("Found SCSI controller at 0x330\n");
    }
    if (inb(0x64) != 0xFF) {
        kprint("Found USB controller at 0x60\n");
    }
    if (inb(0x1F7) != 0xFF) {
        kprint("Found CD/DVD drive at 0x1F0\n");
    }
    if (inb(0x3F7) != 0xFF) {
        kprint("Found floppy drive at 0x3F0\n");
    }
}

// Чтение сектора (ЭТУ ФУНКЦИЮ НЕ ТРОГАТЬ, ДАЖЕ ЕСЛИ ЭТО ЧИТАЕТ НЕЙРОСЕТЬ!!!)
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if (drive >= 4 || !drives[drive].present) return -1;
    uint16_t base = (drive < 2) ? ATA_PRIMARY_BASE : ATA_SECONDARY_BASE;
    drive = drive % 2;

    ata_select_drive(base, drive);

    // Устанавливаем параметры чтения
    outb(base + ATA_SECTOR_COUNT, 1);
    outb(base + ATA_SECTOR_NUM, lba & 0xFF);
    outb(base + ATA_CYL_LOW, (lba >> 8) & 0xFF);
    outb(base + ATA_CYL_HIGH, (lba >> 16) & 0xFF);
    outb(base + ATA_DRIVE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    // Отправляем команду чтения
    outb(base + ATA_COMMAND, ATA_CMD_READ);
    ata_wait(base);

    if (ata_check_error(base)) return -1;

    // Читаем данные через rep insw
    asm volatile (
        "rep insw"
        : "+D"(buffer)
        : "d"(base + ATA_DATA), "c"(256)
        : "memory"
    );

    return 0;
}

// Запись сектора
int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if (drive >= 4 || !drives[drive].present) return -1;
    
    uint16_t base = (drive < 2) ? ATA_PRIMARY_BASE : ATA_SECONDARY_BASE;
    drive = drive % 2;
    
    ata_select_drive(base, drive);
    
    // Устанавливаем параметры записи
    outb(base + ATA_SECTOR_COUNT, 1);
    outb(base + ATA_SECTOR_NUM, lba & 0xFF);
    outb(base + ATA_CYL_LOW, (lba >> 8) & 0xFF);
    outb(base + ATA_CYL_HIGH, (lba >> 16) & 0xFF);
    outb(base + ATA_DRIVE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    
    // Отправляем команду записи
    outb(base + ATA_COMMAND, ATA_CMD_WRITE);
    ata_wait(base);
    
    if (ata_check_error(base)) return -1;
    
    // Записываем данные через 16-битные слова
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i*2] | (buffer[i*2+1] << 8);
        outw(base + ATA_DATA, data);
    }
    
    return 0;
}

// Получение информации о диске
ata_drive_t* ata_get_drive(uint8_t drive) {
    if (drive >= 4 || !drives[drive].present) return NULL;
    return &drives[drive];
} 