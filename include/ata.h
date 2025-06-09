#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA порты
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_SECONDARY_BASE  0x170
#define ATA_CONTROL_BASE    0x3F6

// Регистры ATA
#define ATA_DATA        0x00
#define ATA_ERROR       0x01
#define ATA_FEATURES    0x01
#define ATA_SECTOR_COUNT 0x02
#define ATA_SECTOR_NUM  0x03
#define ATA_CYL_LOW     0x04
#define ATA_CYL_HIGH    0x05
#define ATA_DRIVE       0x06
#define ATA_STATUS      0x07
#define ATA_COMMAND     0x07

// Команды ATA
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC

// Статусные биты
#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DF       0x20
#define ATA_SR_DSC      0x10
#define ATA_SR_DRQ      0x08
#define ATA_SR_CORR     0x04
#define ATA_SR_IDX      0x02
#define ATA_SR_ERR      0x01

// Структура для хранения информации о диске
typedef struct {
    uint8_t present;        // Присутствует ли диск
    uint8_t type;           // Тип диска (0 - нет, 1 - ATA, 2 - ATAPI)
    uint32_t sectors;       // Количество секторов
    uint32_t size;          // Размер в байтах
    char name[40];          // Имя диска
} ata_drive_t;

// Функции драйвера
void ata_init();
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer);
int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t* buffer);
ata_drive_t* ata_get_drive(uint8_t drive);

#endif // ATA_H 