#include "iso9660.h"
#include "atapi.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ISO9660_SECTOR_SIZE 2048
#define ISO9660_PVD_SECTOR 16

struct iso9660_pvd g_pvd;
uint32_t g_root_dir_lba = 0;
uint32_t g_root_dir_size = 0;
int iso9660_atapi_devnum = 2; // по умолчанию 2 (secondary master)

// Вспомогательная функция для сравнения имён файлов (без учёта ;1)
static int iso9660_namecmp(const char* iso_name, size_t iso_len, const char* path) {
    size_t path_len = strlen(path);
    if (iso_len < path_len) return 0;
    for (size_t i = 0; i < path_len; ++i) {
        if (iso_name[i] != path[i]) return 0;
    }
    // Проверяем, что дальше в iso_name идёт ';' или конец
    if (iso_len > path_len && iso_name[path_len] != ';') return 0;
    return 1;
}

// Монтирование ISO9660: читаем PVD и корневой каталог
int iso9660_mount_dev(int devnum) {
    iso9660_atapi_devnum = devnum;
    if (atapi_read_device(iso9660_atapi_devnum, ISO9660_PVD_SECTOR, 1, &g_pvd) != 0) return -1;
    if (g_pvd.type != 1 || strncmp(g_pvd.id, "CD001", 5) != 0) {kprintf("Invalid sigrature\n"); return -2;}
    // Корневой каталог: байты 156-189 в PVD
    uint8_t* root_dir = ((uint8_t*)&g_pvd) + 156;
    g_root_dir_lba = root_dir[2] | (root_dir[3]<<8) | (root_dir[4]<<16) | (root_dir[5]<<24);
    g_root_dir_size = root_dir[10] | (root_dir[11]<<8) | (root_dir[12]<<16) | (root_dir[13]<<24);
    return 0;
}

int iso9660_mount() {
    return iso9660_mount_dev(iso9660_atapi_devnum);
}

// Поиск файла только в корневом каталоге (упрощённо)
int iso9660_find(const char* path, uint32_t* lba, uint32_t* size) {
    uint8_t sector[ISO9660_SECTOR_SIZE];
    if (atapi_read_device(iso9660_atapi_devnum, g_root_dir_lba, 1, sector) != 0) return -1;
    size_t offset = 0;
    while (offset < ISO9660_SECTOR_SIZE) {
        uint8_t len = sector[offset];
        if (len == 0) break;
        uint8_t name_len = sector[offset+32];
        char* name = (char*)&sector[offset+33];
        if (iso9660_namecmp(name, name_len, path)) {
            *lba = sector[offset+2] | (sector[offset+3]<<8) | (sector[offset+4]<<16) | (sector[offset+5]<<24);
            *size = sector[offset+10] | (sector[offset+11]<<8) | (sector[offset+12]<<16) | (sector[offset+13]<<24);
            return 0;
        }
        offset += len;
    }
    return -2;
}

// Чтение файла по пути (только из корня)
int iso9660_read(const char* path, void* buffer, uint32_t max_size) {
    uint32_t lba, size;
    if (iso9660_find(path, &lba, &size) != 0) return -1;
    if (size > max_size) size = max_size;
    uint32_t sectors = (size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    if (atapi_read_device(iso9660_atapi_devnum, lba, sectors, buffer) != 0) return -2;
    return size;
}

// ... реализация будет добавлена ... 