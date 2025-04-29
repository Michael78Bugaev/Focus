#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA порты
#define ATA_PRIMARY_IO     0x1F0
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_SECONDARY_IO   0x170
#define ATA_SECONDARY_CTRL 0x376

// Команды ATA
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// Статус регистр биты
#define ATA_SR_ERR     0x01
#define ATA_SR_DRQ     0x08
#define ATA_SR_DF      0x20
#define ATA_SR_BSY     0x80

// Типы устройств
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

// Максимальное количество дисков
#define MAX_ATA_DEVICES 4

// Типы устройств
#define DEVICE_TYPE_NONE   0
#define DEVICE_TYPE_ATA    1
#define DEVICE_TYPE_ATAPI  2

// Структура для хранения информации об устройстве
typedef struct {
    uint8_t exists;
    uint8_t type;
    uint8_t bus;        // 0 - primary, 1 - secondary
    uint8_t drive;      // 0 - master, 1 - slave
    uint16_t signature;
    uint16_t capabilities;
    uint32_t command_sets;
    uint32_t size;
    char model[41];
} ata_device_t;

// Глобальный массив найденных устройств
extern ata_device_t ata_devices[MAX_ATA_DEVICES];
extern int ata_device_count;

// Функции драйвера
void ata_init();
int ata_identify(uint8_t bus, uint8_t drive);
int ata_read_sectors(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer);
int ata_write_sectors(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer);
int atapi_read(uint8_t device_id, uint32_t lba, uint8_t num_sectors, uint16_t *buffer);

// Вспомогательные функции
int ata_wait_busy(uint16_t base);
int ata_wait_drq(uint16_t base);
uint8_t ata_read_status(uint16_t base);
void ata_write_command(uint16_t base, uint8_t command);
void ata_write_lba(uint16_t base, uint32_t lba, uint8_t drive);

// Функции для работы с устройствами
int ata_get_device_count();
const char* ata_get_device_name(uint8_t device_id);
uint32_t ata_get_device_size(uint8_t device_id);

#endif // ATA_H 