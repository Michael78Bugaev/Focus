#ifndef FS_H
#define FS_H

#include <stdint.h>

// Типы файловых систем
#define FS_TYPE_FAT32 1
#define FS_TYPE_ISO9660 2

// Структура для описания файловой системы
typedef struct {
    uint8_t type;           // Тип ФС (FAT32 или ISO9660)
    uint8_t drive;          // Номер диска
    void* fs_data;          // Указатель на специфичные данные ФС
} filesystem_t;

// Структура для описания файла/директории
typedef struct {
    char name[256];         // Имя файла
    uint32_t size;          // Размер файла
    uint8_t is_dir;         // Это директория?
    void* fs_specific;      // Специфичные данные ФС
} fs_entry_t;

// Функции для работы с файловой системой
filesystem_t* fs_mount(uint8_t drive);
int fs_read_dir(filesystem_t* fs, fs_entry_t* entries, int max_entries);
int fs_read_file(filesystem_t* fs, const char* path, uint8_t* buf, uint32_t size);
int fs_write_file(filesystem_t* fs, const char* path, const uint8_t* buf, uint32_t size);
int fs_change_dir(filesystem_t* fs, const char* path);
char* fs_get_path(filesystem_t* fs);

// Глобальные переменные
extern filesystem_t* current_fs;  // Текущая файловая система
extern uint8_t current_drive;     // Текущий диск

#endif 