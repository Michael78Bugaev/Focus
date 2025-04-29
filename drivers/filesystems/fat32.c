#include <fat32.h>
#include <ata.h>
#include <string.h>
#include <kernel.h>
#include <mem.h>

// Глобальные переменные
fat32_fs_t fat32_fs;
uint16_t* fat_buffer;
uint8_t* sector_buffer;

// Преобразование символа в верхний регистр
static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

// Вспомогательные функции
uint32_t cluster_to_sector(uint32_t cluster) {
    return fat32_fs.first_data_sector + (cluster - 2) * fat32_fs.sectors_per_cluster;
}

void fat32_read_fat_sector(uint32_t fat_sector) {
    ata_read_sectors(fat32_fs.disk_id, fat32_fs.first_fat_sector + fat_sector, 1, fat_buffer);
}

void fat32_write_fat_sector(uint32_t fat_sector) {
    ata_write_sectors(fat32_fs.disk_id, fat32_fs.first_fat_sector + fat_sector, 1, fat_buffer);
}

uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_sector = current_cluster / (FAT32_SECTOR_SIZE / 4);
    uint32_t fat_offset = current_cluster % (FAT32_SECTOR_SIZE / 4);
    
    fat32_read_fat_sector(fat_sector);
    return fat_buffer[fat_offset] & 0x0FFFFFFF;
}

static void fat32_set_next_cluster(uint32_t cluster, uint32_t next_cluster) {
    uint32_t fat_sector = cluster / (FAT32_SECTOR_SIZE / 4);
    uint32_t fat_offset = cluster % (FAT32_SECTOR_SIZE / 4);
    
    fat32_read_fat_sector(fat_sector);
    fat_buffer[fat_offset] = (fat_buffer[fat_offset] & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
    fat32_write_fat_sector(fat_sector);
}

uint32_t fat32_allocate_cluster() {
    for (uint32_t cluster = 2; cluster < fat32_fs.total_clusters; cluster++) {
        if (fat32_get_next_cluster(cluster) == 0) {
            fat32_set_next_cluster(cluster, FAT32_EOC);
            return cluster;
        }
    }
    return 0;
}

void fat32_free_cluster(uint32_t cluster) {
    fat32_set_next_cluster(cluster, 0);
}

static void fat32_free_cluster_chain(uint32_t start_cluster) {
    while (start_cluster >= 2 && start_cluster < FAT32_EOC) {
        uint32_t next_cluster = fat32_get_next_cluster(start_cluster);
        fat32_free_cluster(start_cluster);
        start_cluster = next_cluster;
    }
}

// Конвертация имени файла в формат FAT32 (8.3)
static void fat32_filename_to_83(const char* filename, char* name83) {
    int i, j;
    
    // Очищаем имя
    memset(name83, ' ', 11);
    
    // Копируем имя (до 8 символов или до точки)
    for (i = 0; i < 8 && filename[i] && filename[i] != '.'; i++) {
        name83[i] = to_upper(filename[i]);
    }
    
    // Ищем расширение
    while (filename[i] && filename[i] != '.') i++;
    if (filename[i] == '.') {
        i++;
        // Копируем расширение (до 3 символов)
        for (j = 0; j < 3 && filename[i]; i++, j++) {
            name83[8 + j] = to_upper(filename[i]);
        }
    }
}

// Конвертация имени из формата FAT32 (8.3) в обычную строку
static void fat32_83_to_filename(const char* name83, char* filename) {
    int i, j;
    
    // Копируем имя
    for (i = 0; i < 8 && name83[i] != ' '; i++) {
        filename[i] = name83[i];
    }
    
    // Если есть расширение
    if (name83[8] != ' ') {
        filename[i++] = '.';
        for (j = 8; j < 11 && name83[j] != ' '; j++) {
            filename[i++] = name83[j];
        }
    }
    filename[i] = '\0';
}

// Поиск записи в директории
static int fat32_find_entry(uint32_t dir_cluster, const char* name, fat32_direntry_t* entry) {
    char name83[11];
    fat32_filename_to_83(name, name83);
    
    while (dir_cluster >= 2 && dir_cluster < FAT32_EOC) {
        uint32_t sector = cluster_to_sector(dir_cluster);
        for (uint32_t i = 0; i < fat32_fs.sectors_per_cluster; i++) {
            ata_read_sectors(fat32_fs.disk_id, sector + i, 1, (uint16_t*)sector_buffer);
            
            fat32_direntry_t* dir = (fat32_direntry_t*)sector_buffer;
            for (uint32_t j = 0; j < FAT32_SECTOR_SIZE / sizeof(fat32_direntry_t); j++) {
                if (dir[j].name[0] == 0) {
                    return 0; // Конец директории
                }
                if (dir[j].name[0] != 0xE5 && // Не удаленная запись
                    !(dir[j].attributes & FAT32_ATTR_LONG_NAME) && // Не длинное имя
                    memcmp(dir[j].name, name83, 11) == 0) {
                    *entry = dir[j];
                    return 1;
                }
            }
        }
        dir_cluster = fat32_get_next_cluster(dir_cluster);
    }
    return 0;
}

// Инициализация FAT32
int fat32_init(uint8_t disk_id) {
    fat32_fs.disk_id = disk_id;
    
    // Выделяем буферы
    fat_buffer = (uint16_t*)malloc(FAT32_SECTOR_SIZE);
    sector_buffer = (uint8_t*)malloc(FAT32_SECTOR_SIZE);
    
    if (!fat_buffer || !sector_buffer) {
        kprint("Failed to allocate FAT32 buffers\n");
        return 0;
    }
    
    // Читаем загрузочный сектор
    fat32_boot_t* boot = (fat32_boot_t*)sector_buffer;
    if (!ata_read_sectors(disk_id, 0, 1, (uint16_t*)boot)) {
        kprint("Failed to read boot sector\n");
        return 0;
    }
    
    // Проверяем сигнатуру FAT32
    if (boot->boot_signature != 0x29 || 
        memcmp(boot->fs_type, "FAT32   ", 8) != 0) {
        kprintf("Invalid FAT32 signature: %s\n", boot->fs_type);
        return 0;
    }
    
    // Инициализируем параметры файловой системы
    fat32_fs.first_fat_sector = boot->reserved_sectors;
    fat32_fs.fat_size = boot->fat_size_32;
    fat32_fs.sectors_per_cluster = boot->sectors_per_cluster;
    fat32_fs.root_cluster = boot->root_cluster;
    fat32_fs.first_data_sector = boot->reserved_sectors + 
                                boot->num_fats * boot->fat_size_32;
    fat32_fs.total_clusters = (boot->total_sectors_32 - fat32_fs.first_data_sector) / 
                             fat32_fs.sectors_per_cluster;
    
    // Копируем метку тома
    memcpy(fat32_fs.volume_label, boot->volume_label, 11);
    fat32_fs.volume_label[11] = '\0';
    
    // Устанавливаем текущую директорию в корневую
    fat32_fs.current_directory_cluster = fat32_fs.root_cluster;
    strcpy(fat32_fs.current_directory, "0:\\");
    
    kprintf("FAT32 initialized: %s\n", fat32_fs.volume_label);
    return 1;
}

// Форматирование диска в FAT32
int fat32_format(uint8_t disk_id, const char* volume_label) {
    // TODO: Реализовать форматирование
    return 0;
}

// Изменение текущей директории
int fat32_change_directory(const char* path) {
    char temp_path[FAT32_MAX_PATH];
    uint32_t current_cluster = fat32_fs.current_directory_cluster;
    
    // Если путь начинается с диска
    if (path[0] == '0' && path[1] == ':') {
        current_cluster = fat32_fs.root_cluster;
        path += 2;
    }
    
    // Если путь начинается с корня
    if (path[0] == '\\') {
        current_cluster = fat32_fs.root_cluster;
        path++;
    }
    
    // Разбираем путь
    strncpy(temp_path, path, FAT32_MAX_PATH);
    char* token = strtok(temp_path, "\\");
    
    while (token) {
        if (strcmp(token, ".") == 0) {
            // Текущая директория
        } else if (strcmp(token, "..") == 0) {
            // Родительская директория
            if (current_cluster != fat32_fs.root_cluster) {
                // TODO: Найти родительскую директорию
            }
        } else {
            // Ищем поддиректорию
            fat32_direntry_t entry;
            if (!fat32_find_entry(current_cluster, token, &entry) ||
                !(entry.attributes & FAT32_ATTR_DIRECTORY)) {
                return 0;
            }
            current_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
        }
        token = strtok(NULL, "\\");
    }
    
    // Обновляем текущую директорию
    fat32_fs.current_directory_cluster = current_cluster;
    
    // Обновляем путь
    if (path[0] == '0' && path[1] == ':') {
        strcpy(fat32_fs.current_directory, path);
    } else if (path[0] == '\\') {
        strcpy(fat32_fs.current_directory, "0:");
        strcat(fat32_fs.current_directory, path);
    } else {
        if (fat32_fs.current_directory[strlen(fat32_fs.current_directory)-1] != '\\') {
            strcat(fat32_fs.current_directory, "\\");
        }
        strcat(fat32_fs.current_directory, path);
    }
    
    return 1;
}

// Получение текущей директории
int fat32_get_current_directory(char* path, int max_len) {
    strncpy(path, fat32_fs.current_directory, max_len);
    return 1;
}

// Листинг директории
int fat32_list_directory(const char* path) {
    uint32_t dir_cluster;
    
    // Если путь не указан, используем текущую директорию
    if (!path || !*path) {
        dir_cluster = fat32_fs.current_directory_cluster;
    } else {
        // TODO: Найти директорию по пути
        return 0;
    }
    
    while (dir_cluster >= 2 && dir_cluster < FAT32_EOC) {
        uint32_t sector = cluster_to_sector(dir_cluster);
        for (uint32_t i = 0; i < fat32_fs.sectors_per_cluster; i++) {
            ata_read_sectors(fat32_fs.disk_id, sector + i, 1, (uint16_t*)sector_buffer);
            
            fat32_direntry_t* dir = (fat32_direntry_t*)sector_buffer;
            for (uint32_t j = 0; j < FAT32_SECTOR_SIZE / sizeof(fat32_direntry_t); j++) {
                if (dir[j].name[0] == 0) {
                    return 1; // Конец директории
                }
                if (dir[j].name[0] != 0xE5 && // Не удаленная запись
                    !(dir[j].attributes & FAT32_ATTR_LONG_NAME)) { // Не длинное имя
                    char filename[13];
                    fat32_83_to_filename(dir[j].name, filename);
                    
                    // Выводим информацию о файле/директории
                    if (dir[j].attributes & FAT32_ATTR_DIRECTORY) {
                        kprintf("[DIR]  %s\n", filename);
                    } else {
                        kprintf("[FILE] %s (%d bytes)\n", filename, dir[j].file_size);
                    }
                }
            }
        }
        dir_cluster = fat32_get_next_cluster(dir_cluster);
    }
    
    return 1;
}

// Создание директории
int fat32_create_directory(const char* path) {
    // TODO: Реализовать создание директории
    return 0;
}

// Создание файла
int fat32_create_file(const char* path) {
    // TODO: Реализовать создание файла
    return 0;
}

// Удаление файла или директории
int fat32_delete(const char* path) {
    // TODO: Реализовать удаление
    return 0;
}

// Переименование файла или директории
int fat32_rename(const char* old_path, const char* new_path) {
    // TODO: Реализовать переименование
    return 0;
}

// Открытие файла
int fat32_open(const char* path, fat32_file_t* file) {
    // TODO: Реализовать открытие файла
    return 0;
}

// Чтение из файла
int fat32_read(fat32_file_t* file, void* buffer, uint32_t size) {
    // TODO: Реализовать чтение файла
    return 0;
}

// Запись в файл
int fat32_write(fat32_file_t* file, const void* buffer, uint32_t size) {
    // TODO: Реализовать запись в файл
    return 0;
}

// Закрытие файла
int fat32_close(fat32_file_t* file) {
    // TODO: Реализовать закрытие файла
    return 0;
}
