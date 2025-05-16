#pragma once
#include <stdint.h>
#include <stddef.h>

#define ISO9660_SECTOR_SIZE 2048

// Структура Primary Volume Descriptor (PVD)
struct iso9660_pvd {
    uint8_t type;
    char id[5];
    uint8_t version;
    uint8_t unused1;
    char system_id[32];
    char volume_id[32];
    uint8_t unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    // ... остальные поля не включены для краткости ...
    uint8_t unused3[2048-80];
};

extern uint32_t g_root_dir_lba;
extern uint32_t g_root_dir_size;
extern int iso9660_atapi_devnum;

int iso9660_mount();
int iso9660_mount_dev(int devnum);
int iso9660_find(const char* path, uint32_t* lba, uint32_t* size);
int iso9660_read(const char* path, void* buffer, uint32_t max_size); 