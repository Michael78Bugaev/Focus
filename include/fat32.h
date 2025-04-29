#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

// Константы FAT32
#define FAT32_SECTOR_SIZE     512
#define FAT32_CLUSTER_SIZE    4096  // 8 секторов
#define FAT32_EOC            0x0FFFFFF8  // End of cluster chain
#define FAT32_BAD_CLUSTER    0x0FFFFFF7
#define FAT32_MAX_PATH       260
#define FAT32_MAX_NAME       255
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LONG_NAME (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)

// Структура загрузочного сектора FAT32
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t boot_signature2;
} __attribute__((packed)) fat32_boot_t;

// Структура записи каталога FAT32
typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_direntry_t;

// Структура длинного имени файла
typedef struct {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attribute;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster;
    uint16_t name3[2];
} __attribute__((packed)) fat32_longname_t;

// Структура для работы с FAT32
typedef struct {
    uint8_t  disk_id;
    uint32_t first_data_sector;
    uint32_t first_fat_sector;
    uint32_t sectors_per_cluster;
    uint32_t root_cluster;
    uint32_t fat_size;
    uint32_t total_clusters;
    char     volume_label[12];
    uint32_t current_directory_cluster;
    char     current_directory[FAT32_MAX_PATH];
} fat32_fs_t;

// Структура для работы с файлом/директорией
typedef struct {
    char     name[FAT32_MAX_NAME];
    uint32_t cluster;
    uint32_t size;
    uint8_t  attributes;
    uint16_t creation_date;
    uint16_t creation_time;
    uint16_t last_write_date;
    uint16_t last_write_time;
} fat32_file_t;

// Инициализация FAT32
int fat32_init(uint8_t disk_id);

// Форматирование диска в FAT32
int fat32_format(uint8_t disk_id, const char* volume_label);

// Функции для работы с путями
int fat32_change_directory(const char* path);
int fat32_get_current_directory(char* path, int max_len);

// Функции для работы с файлами и директориями
int fat32_list_directory(const char* path);
int fat32_create_directory(const char* path);
int fat32_create_file(const char* path);
int fat32_delete(const char* path);
int fat32_rename(const char* old_path, const char* new_path);

// Функции для работы с файлами
int fat32_open(const char* path, fat32_file_t* file);
int fat32_read(fat32_file_t* file, void* buffer, uint32_t size);
int fat32_write(fat32_file_t* file, const void* buffer, uint32_t size);
int fat32_close(fat32_file_t* file);

// Вспомогательные функции
uint32_t fat32_get_next_cluster(uint32_t current_cluster);
uint32_t fat32_allocate_cluster();
void fat32_free_cluster(uint32_t cluster);

#endif // FAT32_H
