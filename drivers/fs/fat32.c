#include <fat32.h>
#include <ata.h>
#include <string.h>

fat32_bpb_t fat32_bpb;
uint32_t fat_start, cluster_begin_lba, sectors_per_fat, root_dir_first_cluster;
static uint8_t fat_buffer[512];
uint32_t current_dir_cluster;

// Локальная функция для перевода символа в верхний регистр
static char my_toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

int fat32_mount(uint8_t drive) {
    uint8_t sector[512];
    if (ata_read_sector(drive, 0, sector) != 0)
        return -1;
    for (int i = 0; i < sizeof(fat32_bpb); i++)
        ((uint8_t*)&fat32_bpb)[i] = sector[i];

    sectors_per_fat = fat32_bpb.table_size_32;
    fat_start = fat32_bpb.reserved_sector_count;
    cluster_begin_lba = fat_start + fat32_bpb.table_count * sectors_per_fat;
    root_dir_first_cluster = fat32_bpb.root_cluster;
    kprintf("BPB root_cluster: %u\n", fat32_bpb.root_cluster);
    if (root_dir_first_cluster == 0) {
        root_dir_first_cluster = 2;
        kprint("Warning: root_cluster in BPB is 0, using 2\n");
    }
    current_dir_cluster = root_dir_first_cluster;
    kprintf("FAT32 parameters:\n");
    kprintf("Sectors per FAT: %d\n", sectors_per_fat);
    kprintf("FAT start: %d\n", fat_start);
    kprintf("Cluster begin (LBA): %d\n", cluster_begin_lba);
    kprintf("Root directory first cluster: %d\n", root_dir_first_cluster);
    return 0;
}

uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    return cluster_begin_lba + (cluster - 2) * fat32_bpb.sectors_per_cluster;
}

uint32_t fat32_get_next_cluster(uint8_t drive, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (ata_read_sector(drive, fat_sector, fat_buffer) != 0)
        return 0x0FFFFFFF; // error

    uint32_t next = *(uint32_t*)&fat_buffer[ent_offset];
    return next & 0x0FFFFFFF;
}

int fat32_read_dir(uint8_t drive, uint32_t cluster, fat32_dir_entry_t* entries, int max_entries) {
    int entry_count = 0;
    uint8_t sector[512];
    do {
        for (uint8_t s = 0; s < fat32_bpb.sectors_per_cluster; s++) {
            uint32_t lba = fat32_cluster_to_lba(cluster) + s;
            if (ata_read_sector(drive, lba, sector) != 0)
                return -1;
            for (int i = 0; i < 512; i += 32) {
                if (entry_count >= max_entries) return entry_count;
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)&sector[i];
                if (entry->name[0] == 0x00) return entry_count; // конец каталога
                if (entry->name[0] == 0xE5) continue; // удалённый
                for (int b = 0; b < 32; ++b)
                    ((uint8_t*)&entries[entry_count])[b] = ((uint8_t*)entry)[b];
                entry_count++;
            }
        }
        cluster = fat32_get_next_cluster(drive, cluster);
    } while (cluster < 0x0FFFFFF8);
    return entry_count;
}

int fat32_read_file(uint8_t drive, uint32_t first_cluster, uint8_t* buf, uint32_t size) {
    uint32_t cluster = first_cluster;
    uint32_t bytes_read = 0;
    uint8_t sector[512];
    while (bytes_read < size && cluster < 0x0FFFFFF8) {
        for (uint8_t s = 0; s < fat32_bpb.sectors_per_cluster; s++) {
            uint32_t lba = fat32_cluster_to_lba(cluster) + s;
            if (ata_read_sector(drive, lba, sector) != 0)
                return -1;
            uint32_t to_copy = (size - bytes_read > 512) ? 512 : (size - bytes_read);
            strncpy(buf + bytes_read, sector, to_copy);
            bytes_read += to_copy;
            if (bytes_read >= size) break;
        }
        cluster = fat32_get_next_cluster(drive, cluster);
    }
    return bytes_read;
}

// Функция для разрешения пути (абсолютного или относительного)
static int fat32_resolve_path(uint8_t drive, const char* path, uint32_t* target_cluster) {
    if (strncmp(path, "0:\\", 3) == 0 || strncmp(path, "0:/", 3) == 0) {
        // Абсолютный путь, начинаем с корневого каталога
        *target_cluster = root_dir_first_cluster;
        path += 3;
    } else {
        // Относительный путь, начинаем с текущей директории
        *target_cluster = current_dir_cluster;
    }

    // Если путь пустой, возвращаем текущий кластер
    if (*path == '\0') return 0;

    // Разбиваем путь на компоненты
    char component[12];
    while (*path) {
        // Пропускаем разделители
        while (*path == '\\' || *path == '/') path++;
        if (*path == '\0') break;

        // Извлекаем компонент пути
        int i = 0;
        while (*path && *path != '\\' && *path != '/' && i < 11) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        // Обрабатываем специальные компоненты
        if (strcmp(component, ".") == 0) {
            // Текущая директория, ничего не делаем
            continue;
        } else if (strcmp(component, "..") == 0) {
            // Родительская директория
            if (*target_cluster == root_dir_first_cluster) {
                // Если мы в корневом каталоге, то остаёмся в нём
                return -3;
            }
            // Ищем запись .. в текущем каталоге
            fat32_dir_entry_t entries[16];
            int count = fat32_read_dir(drive, *target_cluster, entries, 16);
            for (int j = 0; j < count; j++) {
                if (strcmp(entries[j].name, "..") == 0) {
                    *target_cluster = entries[j].first_cluster_low | (entries[j].first_cluster_high << 16);
                    break;
                }
            }
        } else {
            // Обычная директория
            fat32_dir_entry_t entries[16];
            int count = fat32_read_dir(drive, *target_cluster, entries, 16);
            int found = 0;
            for (int j = 0; j < count; j++) {
                // Подготовка component к формату 8.3
                char fatname[11];
                int clen = strlen(component);
                int dot = -1;
                for (int i = 0; i < clen; i++) {
                    if (component[i] == '.') { dot = i; break; }
                }
                memset(fatname, ' ', 11);
                if (dot == -1) {
                    for (int i = 0; i < clen && i < 8; i++)
                        fatname[i] = my_toupper(component[i]);
                } else {
                    for (int i = 0; i < dot && i < 8; i++)
                        fatname[i] = my_toupper(component[i]);
                    for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++)
                        fatname[j] = my_toupper(component[i]);
                }
                // Сравниваем с entries[j].name
                int match = 1;
                for (int k = 0; k < 11; k++) {
                    if (fatname[k] != entries[j].name[k]) { match = 0; break; }
                }
                if (match) {
                    if (entries[j].attr & 0x10) {
                        *target_cluster = entries[j].first_cluster_low | (entries[j].first_cluster_high << 16);
                        found = 1;
                        break;
                    }
                    return -2; // Это не директория
                }
            }
            if (!found) return -1; // Директория не найдена
        }
    }
    return 0;
}

// Обновлённая функция для перехода в указанную директорию с подробной ошибкой
// Возвращает: 0 - успех, -1 - не найдена, -2 - не директория, -3 - уже в корне
int fat32_change_dir(uint8_t drive, const char* path) {
    uint32_t target_cluster;
    int res = fat32_resolve_path(drive, path, &target_cluster);
    if (res == 0) {
        current_dir_cluster = target_cluster;
        return 0;
    } else if (res == -1) {
        return -1; // Нет такой директории
    } else if (res == -2) {
        return -2; // Это не директория
    } else if (res == -3) {
        return -3; // Уже в корне
    }
    return -1;
}