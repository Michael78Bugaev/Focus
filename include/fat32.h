#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  table_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t table_size_16;
    uint16_t sectors_per_track;
    uint16_t head_side_count;
    uint32_t hidden_sector_count;
    uint32_t total_sectors_32;
    // FAT32 Extended
    uint32_t table_size_32;
    uint16_t ext_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_BS_sector;
    uint8_t  reserved_0[12];
    uint8_t  drive_number;
    uint8_t  reserved_1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fat_type_label[8];
} fat32_bpb_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_dir_entry_t;
#pragma pack(pop)

_Static_assert(sizeof(fat32_dir_entry_t) == 32, "fat32_dir_entry_t must be 32 bytes");

int fat32_mount(uint8_t drive);
uint32_t fat32_cluster_to_lba(uint32_t cluster);
uint32_t fat32_get_next_cluster(uint8_t drive, uint32_t cluster);
int fat32_read_dir(uint8_t drive, uint32_t cluster, fat32_dir_entry_t* entries, int max_entries);
int fat32_read_file(uint8_t drive, uint32_t first_cluster, uint8_t* buf, uint32_t size);

#endif // FAT32_H 