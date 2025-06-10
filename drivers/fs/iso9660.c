#include "iso9660.h"
#include "atapi.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mem.h>

#define ISO9660_SECTOR_SIZE 2048
#define ISO9660_PVD_SECTOR 16

struct iso9660_pvd g_pvd;
uint32_t g_root_dir_lba = 0;
uint32_t g_root_dir_size = 0;
// Default to primary slave (0:1), which is typical QEMU CD-ROM configuration
int iso9660_atapi_devnum = 1; // was 2 (secondary master)

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

// Helper: search for a name within a directory (dir_lba, dir_size)
static int iso9660_find_in_dir(const char* name, uint32_t* out_lba, uint32_t* out_size, uint32_t dir_lba, uint32_t dir_size) {
    uint32_t sectors = (dir_size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    uint8_t* sector = malloc(ISO9660_SECTOR_SIZE);
    if (!sector) return -1;
    for (uint32_t i = 0; i < sectors; i++) {
        if (atapi_read_device(iso9660_atapi_devnum, dir_lba + i, 1, sector) != 0) continue;
        size_t offset = 0;
        while (offset < ISO9660_SECTOR_SIZE) {
            uint8_t len = sector[offset];
            if (len == 0) break;
            uint8_t name_len = sector[offset+32];
            char* rec_name = (char*)&sector[offset+33];
            if (iso9660_namecmp(rec_name, name_len, name)) {
                *out_lba = sector[offset+2] | (sector[offset+3]<<8) | (sector[offset+4]<<16) | (sector[offset+5]<<24);
                *out_size = sector[offset+10] | (sector[offset+11]<<8) | (sector[offset+12]<<16) | (sector[offset+13]<<24);
                mfree(sector);
                return 0;
            }
            offset += len;
        }
    }
    mfree(sector);
    return -1;
}

// Enhanced iso9660_find: support nested paths using '/'
int iso9660_find(const char* path, uint32_t* lba, uint32_t* size) {
    if (!path || !lba || !size) return -1;
    char* path_copy = malloc(strlen(path) + 1);
    if (!path_copy) return -1;
    strcpy(path_copy, path);
    // Normalize path: uppercase letters and convert backslashes to slashes
    for (char* p = path_copy; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p = *p - ('a' - 'A');
        if (*p == '\\') *p = '/';
    }
    // Strip CDROM:/ prefix if present
    if (strncmp(path_copy, "CDROM:/", 7) == 0) {
        memmove(path_copy, path_copy + 7, strlen(path_copy + 7) + 1);
    }
    char* token = strtok(path_copy, "/");
    uint32_t curr_lba = g_root_dir_lba;
    uint32_t curr_size = g_root_dir_size;
    uint32_t ent_lba = 0;
    uint32_t ent_size = 0;
    while (token) {
        if (iso9660_find_in_dir(token, &ent_lba, &ent_size, curr_lba, curr_size) != 0) {
            mfree(path_copy);
            return -2;
        }
        token = strtok(NULL, "/");
        if (token) {
            curr_lba = ent_lba;
            curr_size = ent_size;
        }
    }
    mfree(path_copy);
    *lba = ent_lba;
    *size = ent_size;
    return 0;
}

// Чтение файла по пути (только из корня)
int iso9660_read(const char* path, void* buffer, uint32_t max_size) {
    uint32_t lba, size;
    if (iso9660_find(path, &lba, &size) != 0) return -1;
    // Calculate how many sectors fit in buffer
    uint32_t max_sectors = max_size / ISO9660_SECTOR_SIZE;
    uint32_t sectors = (size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    if (sectors > max_sectors) sectors = max_sectors;
    if (sectors == 0) return 0;
    for (uint32_t i = 0; i < sectors; i++) {
        if (atapi_read_device(iso9660_atapi_devnum, lba + i, 1,
            (uint8_t*)buffer + i * ISO9660_SECTOR_SIZE) != 0) return -2;
    }
    // Return actual bytes read
    uint32_t bytes = sectors * ISO9660_SECTOR_SIZE;
    if (bytes > size) bytes = size;
    return bytes;
}

// ... реализация будет добавлена ... 