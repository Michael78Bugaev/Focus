#include <fat32.h>
#include <ata.h>
#include <string.h>

fat32_bpb_t fat32_bpb;
static uint32_t fat_start, cluster_begin_lba, sectors_per_fat, root_dir_first_cluster;
static uint8_t fat_buffer[512];

int fat32_mount(uint8_t drive) {
    // Считаем первый сектор (Boot Sector)
    if (ata_read_sector(drive, 0, (uint8_t*)&fat32_bpb) != 0)
        return -1;

    sectors_per_fat = fat32_bpb.table_size_32;
    fat_start = fat32_bpb.reserved_sector_count;
    cluster_begin_lba = fat_start + fat32_bpb.table_count * sectors_per_fat;
    root_dir_first_cluster = fat32_bpb.root_cluster;
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
                strncpy(&entries[entry_count++], entry, 32);
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