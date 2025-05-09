#include <stdint.h>
#include <mem.h>
#include <string.h>
#include <ata.h>
#include <fat32.h>
#include "edit.c"
#include <fcsasm.h>
#include <fcs_vm.h>

int current_disk = 0;
uint8_t * ide_buf; // Buffer for read/write operations

// Prototypes of functions
uint8_t hex_to_int(char c);
uint32_t find_free_cluster(uint8_t drive);

extern int build_path(uint32_t cluster, char path[][9], int max_depth);

// Local function to convert character to uppercase
static char my_toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

// Search for name and parent by cluster: returns 1 if found, 0 if not
static int find_name_and_parent(uint32_t search_cluster, uint32_t current_cluster, char* out_name, uint32_t* out_parent) {
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(0, current_cluster, entries, 32);
    for (int i = 0; i < n; i++) {
        if ((entries[i].attr & 0x10) == 0x10) {
            uint32_t cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            char dbgname[9]; int pos = 0;
            for (int j = 0; j < 8; j++)
                if (entries[i].name[j] != ' ' && entries[i].name[j] != 0)
                    dbgname[pos++] = entries[i].name[j];
            dbgname[pos] = 0;
            if (cl == search_cluster) {
                int pos2 = 0;
                for (int j = 0; j < 8; j++)
                    if (entries[i].name[j] != ' ' && entries[i].name[j] != 0)
                        out_name[pos2++] = entries[i].name[j];
                out_name[pos2] = 0;
                *out_parent = current_cluster;
                return 1;
            }
            if (find_name_and_parent(search_cluster, cl, out_name, out_parent))
                return 1;
        }
    }
    return 0;
}

// Build path up by parents
int build_path(uint32_t cluster, char path[][9], int max_depth) {
    extern uint32_t root_dir_first_cluster;
    int depth = 0;
    uint32_t cur = cluster;
    while (cur != root_dir_first_cluster && depth < max_depth) {
        char name[9];
        uint32_t parent = 0;
        if (!find_name_and_parent(cur, root_dir_first_cluster, name, &parent))
            break;
        for (int i = 0; i < 9; i++) path[depth][i] = name[i];
        cur = parent;
        depth++;
    }
    return depth;
}

// Выполнить .fsc файл как скрипт
void shell_execute_fsc(const char* fname) {
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
    char fatname[12];
    memset(fatname, ' ', 11);
    fatname[11] = 0;
    int clen = strlen(fname);
    int dot = -1;
    for (int i = 0; i < clen; i++) if (fname[i] == '.') { dot = i; break; }
    if (dot == -1) {
        for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(fname[i]);
    } else {
        for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(fname[i]);
        for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(fname[i]);
    }
    int found = 0;
    uint32_t cl = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i].name, fatname, 11) == 0) {
            cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            found = 1;
            break;
        }
    }
    if (!found) {
        kprintf("Cannot read script: %s\n", fname);
        return;
    }
    char buffer[MAX_FILE_SIZE];
    int size = fat32_read_file(current_disk, cl, (uint8_t*)buffer, MAX_FILE_SIZE-1);
    if (size <= 0) {
        kprintf("Cannot read script: %s\n", fname);
        return;
    }
    buffer[size] = 0;
    char* line = strtok(buffer, "\n");
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line && *line != '#') {
            shell_execute(line);
        }
        line = strtok(NULL, "\n");
    }
}

void shell_execute(char *input)
{
    int count;
    char **arg = splitString(input, &count);

    if (count > 0)
    {
        int len = strlen(arg[0]);
        if (len > 4 && strcmp(arg[0] + len - 4, ".fsc") == 0) {
            shell_execute_fsc(arg[0]);
            return;
        }
        if (strcmp(arg[0], "help") == 0)
        {
            kprint("Available commands:\n");
            kprint("    help - show this help\n");
            kprint("    clear - clear screen\n");
            kprint("    disk [n] - select disk (0-3)\n");
            kprint("    read (-l length) [lba] - read sector\n");
            kprint("    write [lba] [-h |-t] [data] - write data\n");
            kprint("    bootsec - create boot sector\n");
            kprint("    list - list available disks\n");
            kprint("    fatmount - mount FAT32 partition\n");
            kprint("    ls - list files in FAT32 root directory\n");
            kprint("    cat - display file content\n");
            kprint("    fatmkfs - create FAT32 filesystem\n");
            kprint("    fatinfo - display FAT32 volume label and filesystem type\n");
            kprint("    touch - create empty file\n");
            kprint("    mkdir - create directory\n");
            kprint("    rm - remove file or directory\n");
            kprint("    edit <filename> - edit file\n");
            kprint("    reboot - reboot the system\n");
            kprint("    shutdown - shut down the system\n");
            kprint("    sleep [milliseconds] - sleep for a specified amount of time\n");
            kprint("    echo [message] - print a message\n");
            kprint("    fcsasm <src.asm> <dst.ex> - compile assembly to executable\n");
            kprint("    fcsasm -l <src.asm> - list labels\n");
            return;
        }
        else if (strcmp(arg[0], "clear") == 0)
        {
            kclear();
            return;
        }
        else if (strcmp(arg[0], "bootsec") == 0)
        {
            uint8_t bootsec[512];
            for (int i = 0; i < 512; i++)
            {
                bootsec[i] = 0;
            }
            bootsec[510] = 0x55;
            bootsec[511] = 0xAA;

            return;
        }
        else if (strcmp(arg[0], "disk") == 0)
        {
            if (count > 1)
            {
                int new_disk = atoi(arg[1]);
                if (new_disk >= 0 && new_disk < 4)
                {
                    ata_drive_t* drive = ata_get_drive(new_disk);
                    if (drive && drive->present) {
                    current_disk = new_disk;
                        kprintf("Selected disk %d:\\\n", current_disk);
                    } else {
                        kprint("Disk not present\n");
                    }
                }
                else
                {
                    kprint("Invalid disk number (must be 0-3)\n");
                }
            }
            return;
        }
        else if (strcmp(arg[0], "write") == 0)
        {
            if (count < 4)
            {
                kprint("Usage: write [data] [sectors] [lba]\n");
                kprint("  data    - hex string to write (e.g. DEADBEEF)\n");
                kprint("  sectors - number of sectors to write\n");
                kprint("  lba     - starting sector number\n");
                return;
            }
            
            char* hex_data = arg[1];
            int num_sectors = atoi(arg[2]);
            int lba = atoi(arg[3]);
            
            // Check parameters
            if (num_sectors <= 0 || num_sectors > 256) {
                kprint("Error: sectors must be between 1 and 256\n");
                return;
            }
            
            if (lba < 0) {
                kprint("Error: LBA must be non-negative\n");
                return;
            }
            
            // Convert hex string to data
            int data_len = strlen(hex_data);
            if (data_len % 2 != 0) {
                kprint("Error: hex data must have even length\n");
                return;
            }
            
            // Clear buffer
            for (int i = 0; i < 512; i++) {
                ide_buf[i] = 0;
            }
            
            // Convert hex to data
            for (int i = 0; i < data_len; i += 2) {
                uint8_t high = hex_to_int(hex_data[i]);
                uint8_t low = hex_to_int(hex_data[i + 1]);
                if (high == 0xFF || low == 0xFF) {
                    kprintf("Error: invalid hex character at position %d\n", i);
                    return;
                }
                ide_buf[i/2] = (high << 4) | low;
            }

            // Write sectors
            for (int i = 0; i < num_sectors; i++) {
                if (ata_write_sector(current_disk, lba + i, ide_buf) != 0) {
                    kprintf("Error writing sector %d\n", lba + i);
                    return;
                }
            }
            
            kprintf("Successfully wrote %d sectors starting at LBA %d\n", num_sectors, lba);
            return;
        }
        else if (strcmp(arg[0], "read") == 0)
        {
            int num_sectors = 0;
            int lba = 0;
            int print_len = 0;
            int arg_shift = 0;
            // Check for -l flag
            if (count > 1 && strcmp(arg[1], "-l") == 0) {
                if (count < 5) {
                    kprint("Usage: read -l [length] [sectors] [lba]\n");
                    return;
                }
                print_len = atoi(arg[2]);
                num_sectors = atoi(arg[3]);
                lba = atoi(arg[4]);
                arg_shift = 2;
            } else {
                if (count < 3) {
                    kprint("Usage: read -l [length] [sectors] [lba]\n");
                    kprint("  length  - number of bytes to read\n");
                    kprint("  sectors - number of sectors to read (always 1)\n");
                    kprint("  lba     - starting sector number\n");
                    return;
                }
                num_sectors = atoi(arg[1]);
                lba = atoi(arg[2]);
            }
            // Check parameters
            if (num_sectors <= 0 || num_sectors > 256) {
                kprint("Error: sectors must be between 1 and 256\n");
                return;
            }
            if (lba < 0) {
                kprint("Error: LBA must be non-negative\n");
                return;
            }
            
            // Read sectors
            for (int i = 0; i < num_sectors; i++) {
                if (ata_read_sector(current_disk, lba + i, ide_buf) != 0) {
                    kprintf("Error reading sector %d\n", lba + i);
                    return;
                }
                
                // Output data in hex+ascii style (16 bytes per line)
                int len = (print_len > 0) ? print_len : 512;
                for (int j = 0; j < len; j += 16) {
                    kprintf("%04X: ", j);
                    // hex
                    for (int k = 0; k < 16; k++) {
                        if (j + k < len)
                            kprintf("%02X ", ide_buf[j + k]);
                        else
                            kprint("   ");
                    }
                    kprint("  ");
                    // ascii
                    for (int k = 0; k < 16; k++) {
                        if (j + k < len) {
                            char c = ide_buf[j + k];
                            if (c >= 32 && c <= 126)
                                kputchar(c, 0x07);
                            else
                                kputchar('.', 0x07);

                        }
                    }
                    kprint("\n");
                }
            }
            
            kprintf("Successfully read %d sectors starting at LBA %d\n", num_sectors, lba);
            return;
        }
        else if (strcmp(arg[0], "list") == 0)
        {
            kprint("Available disks:\n");
            for (int i = 0; i < 4; i++) {
                ata_drive_t* drive = ata_get_drive(i);
                if (drive && drive->present) {
                    kprintf("%d:\\ - %s (%d sectors, %d bytes)\n", 
                           i, drive->name, drive->sectors, drive->size);
                }
            }
            return;
        }
        else if (strcmp(arg[0], "fatmount") == 0)
        {
            if (fat32_mount(current_disk) == 0) {
                kprint("FAT32 success\n");
            } else {
                kprint("Error while mounting FAT32\n");
            }
            return;
        }
        else if (strcmp(arg[0], "fatinfo") == 0)
        {
            extern fat32_bpb_t fat32_bpb;
            char label[12], type[9];
            strncpy(label, fat32_bpb.volume_label, 11);
            label[11] = 0;
            strncpy(type, fat32_bpb.fat_type_label, 8);
            type[8] = 0;
            // Remove trailing spaces
            for (int i = 10; i >= 0; i--) {
                if (label[i] == ' ') label[i] = 0;
                else break;
            }
            for (int i = 7; i >= 0; i--) {
                if (type[i] == ' ') type[i] = 0;
                else break;
            }
            kprintf("Volume label: %s\n", label);
            kprintf("Filesystem:   %s\n", type);
            return;
        } 
        else if (strcmp(arg[0], "ls") == 0)
        {
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0) {
                kprint("Error reading directory\n");
                return;
            }
            
            // Сначала выводим директории
            for (int i = 0; i < n; i++) {
                if (entries[i].name[0] == 0xE5 || entries[i].name[0] == 0) continue;
                char c = entries[i].name[0];
                if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) continue;
                if ((entries[i].attr & 0x0F) == 0x08) continue; // Пропускаем volume label
                
                if ((entries[i].attr & 0x10) == 0x10) { // Это директория
                    char name[13] = {0};
                    int pos = 0;
                    
                    // Имя (8 символов)
                    for (int j = 0; j < 8; j++) {
                        if (entries[i].name[j] != ' ') {
                            name[pos++] = entries[i].name[j];
                        }
                    }
                    
                    // Расширение (3 символа)
                    int has_ext = 0;
                    for (int j = 8; j < 11; j++) {
                        if (entries[i].name[j] != ' ') has_ext = 1;
                    }
                    
                    if (has_ext) {
                        name[pos++] = '.';
                        for (int j = 8; j < 11; j++) {
                            if (entries[i].name[j] != ' ') {
                                name[pos++] = entries[i].name[j];
                            }
                        }
                    }
                    name[pos] = 0;
                    if (name[0] == 0) continue; // не выводим пустые имена
                    
                    // Считаем количество записей в директории
                    fat32_dir_entry_t subentries[32];
                    int subn = fat32_read_dir(current_disk, 
                        ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low, 
                        subentries, 32);
                    if (subn < 0) subn = 0;
                    
                    kprintf(" <DIR>  %s (%d entries)\n", name, subn);
                            }
                        }
            
            // Затем выводим файлы
            for (int i = 0; i < n; i++) {
                if (entries[i].name[0] == 0xE5 || entries[i].name[0] == 0) continue;
                char c = entries[i].name[0];
                if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) continue;
                if ((entries[i].attr & 0x0F) == 0x08) continue; // Пропускаем volume label
                
                if ((entries[i].attr & 0x10) != 0x10) { // Это файл
                char name[13] = {0};
                int pos = 0;
                    
                // Имя (8 символов)
                for (int j = 0; j < 8; j++) {
                        if (entries[i].name[j] != ' ') {
                        name[pos++] = entries[i].name[j];
                }
                    }
                    
                // Расширение (3 символа)
                int has_ext = 0;
                for (int j = 8; j < 11; j++) {
                        if (entries[i].name[j] != ' ') has_ext = 1;
                }
                    
                if (has_ext) {
                    name[pos++] = '.';
                    for (int j = 8; j < 11; j++) {
                            if (entries[i].name[j] != ' ') {
                                name[pos++] = entries[i].name[j];
                            }
                    }
                }
                name[pos] = 0;
                    if (name[0] == 0) continue; // не выводим пустые имена
                    
                    // Выводим размер файла
                    uint32_t size = entries[i].file_size;
                    if (size < 1024) {
                        kprintf(" <FILE> %s (%u bytes)\n", name, size);
                    } else if (size < 1024*1024) {
                        kprintf(" <FILE> %s (%u.%u KB)\n", name, size/1024, (size%1024)/100);
                } else {
                        kprintf(" <FILE> %s (%u.%u MB)\n", name, size/(1024*1024), (size%(1024*1024))/100000);
                    }
                }
            }
            return;
        }
        else if (strcmp(arg[0], "cat") == 0)
        {
            if (count < 2) {
                kprint("Usage: cat [FILENAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0) {
                kprint("Error read directory\n");
                return;
            }
            char *filename = arg[1];
            // --- преобразуем имя в 8.3 формат FAT ---
            char fatname[12];
            memset(fatname, ' ', 11);
            fatname[11] = 0;
            int clen = strlen(filename);
            int dot = -1;
            for (int i = 0; i < clen; i++) if (filename[i] == '.') { dot = i; break; }
            if (dot == -1) {
                for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(filename[i]);
            } else {
                for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(filename[i]);
                for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(filename[i]);
                }
            for (int i = 0; i < n; i++) {
                if (strncmp(entries[i].name, fatname, 11) == 0) {
                    uint32_t cluster = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                    uint32_t size = entries[i].file_size;
                    uint8_t buf[1024];
                    int read = fat32_read_file(current_disk, cluster, buf, (size > 1024) ? 1024 : size);
                    for (int b = 0; b < read; b++) {
                        kputchar(buf[b], 0x07);
                    }
                    kprint("\n");
                    return;
                }
            }
            kprint("File not found\n");
            return;
        }
        else if (strcmp(arg[0], "fatmkfs") == 0)
        {
            // Minimal x86 bootloader (BIOS, 16 bit)
            const uint8_t bootloader_bin[62] = {
                0xEB, 0x3C, 0x90, // jmp short 0x3E
                // OEM and BPB will be filled below
                // offset 0x3E (62)
                0xB4, 0x0E,             // mov ah, 0x0E
                0xBB, 0x07, 0x00,       // mov bx, 0x0007
                0xBE, 0x4E, 0x00,       // mov si, 0x004E
                0xAC,                   // lodsb
                0x3C, 0x00,             // cmp al, 0
                0x74, 0x06,             // je .hang
                0xCD, 0x10,             // int 0x10
                0xEB, 0xF6,             // jmp .loop
                0xF4,                   // hlt
                0xEB, 0xFE              // jmp $
            };
            const char bootmsg[] = "This is not a bootable disk\r\n";
            uint8_t sector[512];
            memset(sector, 0, 512);
            // Insert bootloader
            strncpy(sector, bootloader_bin, sizeof(bootloader_bin));
            strncpy(sector + 0x4E, bootmsg, sizeof(bootmsg));
            // BPB and labels (as before)
            strncpy((char*)&sector[3], "MSDOS5.0", 8); // OEM
            *(uint16_t*)&sector[11] = 512; // bytes per sector
            sector[13] = 1; // sectors per cluster
            *(uint16_t*)&sector[14] = 32; // reserved sectors
            sector[16] = 2; // FAT count
            *(uint16_t*)&sector[17] = 0; // root entries (FAT32)
            *(uint16_t*)&sector[19] = 0; // total sectors 16
            sector[21] = 0xF8; // media
            *(uint16_t*)&sector[22] = 0; // FAT size 16
            *(uint16_t*)&sector[24] = 63; // sectors per track
            *(uint16_t*)&sector[26] = 255; // heads
            *(uint32_t*)&sector[28] = 0; // hidden sectors
            *(uint32_t*)&sector[32] = 65536; // total sectors 32 (example: 32 MB)
            *(uint32_t*)&sector[36] = 123; // FAT size 32
            *(uint16_t*)&sector[44] = 0; // ext flags
            *(uint16_t*)&sector[46] = 0; // FAT version
            *(uint32_t*)&sector[48] = 2; // root cluster
            *(uint16_t*)&sector[52] = 1; // FSInfo
            *(uint16_t*)&sector[54] = 6; // backup boot sector
            sector[64] = 0x80; // drive number
            sector[66] = 0x29; // boot signature
            *(uint32_t*)&sector[67] = 0x12345678; // volume id
            strncpy((char*)&sector[71], "FOCUSOS    ", 11); // volume label
            strncpy((char*)&sector[82], "FAT32   ", 8);      // fat type label
            sector[510] = 0x55; sector[511] = 0xAA;
            // Write Boot Sector
            if (ata_write_sector(current_disk, 0, sector) != 0) {
                kprint("Error writing Boot Sector\n");
                return;
            }
            // FSInfo
            memset(sector, 0, 512);
            *(uint32_t*)&sector[0] = 0x41615252;
            *(uint32_t*)&sector[484] = 0x61417272;
            *(uint32_t*)&sector[488] = 0xFFFFFFFF;
            *(uint32_t*)&sector[492] = 0xFFFFFFFF;
            sector[510] = 0x55; sector[511] = 0xAA;
            if (ata_write_sector(current_disk, 1, sector) != 0) {
                kprint("Error writing FSInfo\n");
                return;
            }
            // Clear FAT and root cluster
            memset(sector, 0, 512);
            for (int i = 0; i < 32; i++) {
                ata_write_sector(current_disk, 32 + i, sector); // root directory
            }
            for (int i = 0; i < 123 * 2; i++) {
                ata_write_sector(current_disk, 32 + 32 + i, sector); // FAT
            }
            kprint("FAT32 created\n");
            return;
        }
        else if (strcmp(arg[0], "touch") == 0)
        {
            if (count < 2) {
                kprint("Usage: touch [FILENAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0 || n >= 32) {
                kprint("Directory full or error\n");
                return;
            }
            // File name (8.3, without dot)
            char name[11];
            memset(name, ' ', 11);
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            if (arg[1][i] == '.') {
                i++;
                for (int k = 0; k < 3 && arg[1][i]; k++, i++) name[8 + k] = my_toupper(arg[1][i]);
            }
            // Find free entry
            uint8_t sector[512];
            uint32_t lba = fat32_cluster_to_lba(current_dir_cluster);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                kprint("Error reading dir\n");
                return;
            }
            for (int off = 0; off < 512; off += 32) {
                if (sector[off] == 0x00 || sector[off] == 0xE5) {
                    // First erase entire sector
                    uint8_t new_sector[512];
                    memset(new_sector, 0, 512);
                    // Copy all entries before current
                    for (int i = 0; i < off; i++) {
                        new_sector[i] = sector[i];
                    }
                    // Copy all entries after current
                    for (int i = off + 32; i < 512; i++) {
                        new_sector[i] = sector[i];
                    }
                    // Write new entry
                    memset(&new_sector[off], ' ', 11);
                    for (int n = 0; n < 11; n++) new_sector[off + n] = name[n];
                    new_sector[off + 11] = 0x20; // attr: archive
                    // --- Выделяем свободный кластер и записываем его ---
                    uint32_t cl = find_free_cluster(current_disk);
                    if (cl == 0) {
                        kprint("No free cluster\n");
                        return;
                    }
                    uint32_t fat_lba = fat_start + (cl * 4) / 512;
                    uint8_t fat_sec[512];
                    if (ata_read_sector(current_disk, fat_lba, fat_sec) != 0) {
                        kprint("Error reading FAT\n");
                        return;
                    }
                    uint32_t fat_off = (cl * 4) % 512;
                    *(uint32_t*)&fat_sec[fat_off] = 0x0FFFFFFF;
                    if (ata_write_sector(current_disk, fat_lba, fat_sec) != 0) {
                        kprint("Error writing FAT\n");
                        return;
                    }
                    *(uint16_t*)(&new_sector[off + 20]) = (uint16_t)((cl >> 16) & 0xFFFF); // high
                    *(uint16_t*)(&new_sector[off + 26]) = (uint16_t)(cl & 0xFFFF); // low
                    // --- конец исправления ---
                    // Write updated sector
                    if (ata_write_sector(current_disk, lba, new_sector) != 0) {
                        kprint("Error writing dir\n");
                        return;
                    }
                    return;
                }
            }
            kprint("No free entry\n");
            return;
        }
        else if (strcmp(arg[0], "mkdir") == 0)
        {
            if (count < 2) {
                kprint("Usage: mkdir [DIRNAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0 || n >= 32) {
                kprint("Directory full or error\n");
                return;
            }
            // Directory name (8.3, without dot)
            char name[11];
            memset(name, ' ', 11);
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            // Find free entry
            uint8_t sector[512];
            uint32_t lba = fat32_cluster_to_lba(current_dir_cluster);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                kprint("Error reading dir\n");
                return;
            }
            // 1. Find free cluster for new directory
            uint32_t new_cl = find_free_cluster(current_disk);
            kprintf("find_free_cluster returned: %u\n", new_cl);
            if (new_cl == 0) {
                kprint("No free cluster for directory\n");
                return;
            }
            // 2. Mark cluster as used (FAT32: 0x0FFFFFFF)
            // (very simple implementation: write to FAT)
            uint32_t fat_lba = fat_start + (new_cl * 4) / 512;
            uint8_t fat_sec[512];
            if (ata_read_sector(current_disk, fat_lba, fat_sec) != 0) {
                kprint("Error reading FAT\n");
                return;
            }
            uint32_t off = (new_cl * 4) % 512;
            *(uint32_t*)&fat_sec[off] = 0x0FFFFFFF;
            if (ata_write_sector(current_disk, fat_lba, fat_sec) != 0) {
                kprint("Error writing FAT\n");
                return;
            }
            // 3. Write directory entry
            for (int off = 0; off < 512; off += 32) {
                if (sector[off] == 0x00 || sector[off] == 0xE5) {
                    // First erase entire sector
                    uint8_t new_sector[512];
                    memset(new_sector, 0, 512);
                    // Copy all entries before current
                    for (int i = 0; i < off; i++) {
                        new_sector[i] = sector[i];
                    }
                    // Copy all entries after current
                    for (int i = off + 32; i < 512; i++) {
                        new_sector[i] = sector[i];
                    }
                    // Write new entry
                    memset(&new_sector[off], ' ', 11);
                    for (int n = 0; n < 11; n++) new_sector[off + n] = name[n];
                    new_sector[off + 11] = 0x10; // attr: directory
                    *(uint16_t*)(&new_sector[off + 20]) = (uint16_t)((new_cl >> 16) & 0xFFFF); // high
                    *(uint16_t*)(&new_sector[off + 26]) = (uint16_t)(new_cl & 0xFFFF); // low
                    // Write updated sector
                    if (ata_write_sector(current_disk, lba, new_sector) != 0) {
                        kprint("Error writing dir\n");
                        return;
                    }
                    // Initialize new cluster with entries '.' and '..'
                    uint8_t newsec[512];
                    memset(newsec, 0, 512);
                    // .
                    memset(&newsec[0], ' ', 11);
                    newsec[0] = '.';
                    newsec[11] = 0x10; // attr: directory
                    *(uint16_t*)(&newsec[20]) = (uint16_t)((new_cl >> 16) & 0xFFFF); // high
                    *(uint16_t*)(&newsec[26]) = (uint16_t)(new_cl & 0xFFFF); // low
                    // ..
                    memset(&newsec[32], ' ', 11);
                    newsec[32] = '.'; newsec[33] = '.';
                    newsec[43] = 0x10; // attr: directory
                    extern uint32_t current_dir_cluster;
                    extern uint32_t root_dir_first_cluster;
                    uint32_t parent_cl = current_dir_cluster;
                    if (parent_cl == root_dir_first_cluster) {
                        parent_cl = root_dir_first_cluster;
                    }
                    *(uint16_t*)(&newsec[52]) = (uint16_t)((parent_cl >> 16) & 0xFFFF); // high
                    *(uint16_t*)(&newsec[58]) = (uint16_t)(parent_cl & 0xFFFF); // low
                    if (ata_write_sector(current_disk, fat32_cluster_to_lba(new_cl), newsec) != 0) {
                        kprint("Error initializing new dir\n");
                        return;
                    }
                    return;
                }
            }
            kprint("No free entry\n");
            return;
        }
        else if (strcmp(arg[0], "rm") == 0)
        {
            int recursive = 0;
            int path_idx = 1;
            if (count > 1 && strcmp(arg[1], "-r") == 0) {
                recursive = 1;
                path_idx = 2;
            }
            if (count <= path_idx) {
                kprint("Usage: rm [-r] [PATH]\n");
                return;
            }
            // Determine path
            const char* path = arg[path_idx];
            uint32_t target_cluster = 0;
            fat32_dir_entry_t entries[32];
            int parent_cluster = 0;
            char fatname[11];
            // Resolve path
            if (strncmp(path, "0:\\", 3) == 0 || strncmp(path, "0:/", 3) == 0) {
                // Absolute path
                parent_cluster = root_dir_first_cluster;
            } else if (strncmp(path, ".\\", 2) == 0 || strncmp(path, "./", 2) == 0) {
                parent_cluster = current_dir_cluster;
                path += 2;
            } else {
                parent_cluster = current_dir_cluster;
            }
            // Break path into components
            char* last = strrchr(path, '\\');
            if (!last) last = strrchr(path, '/');
            const char* name_component = path;
            if (last) name_component = last + 1;
            // Form fatname
            memset(fatname, ' ', 11);
            int clen = strlen(name_component);
            int dot = -1;
            for (int i = 0; i < clen; i++) if (name_component[i] == '.') { dot = i; break; }
            if (dot == -1) {
                for (int i = 0; i < clen && i < 8; i++) fatname[i] = my_toupper(name_component[i]);
            } else {
                for (int i = 0; i < dot && i < 8; i++) fatname[i] = my_toupper(name_component[i]);
                for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = my_toupper(name_component[i]);
            }
            // If there's a path, search parent cluster
            if (last) {
                char parent_path[256];
                strncpy(parent_path, path, last - path);
                parent_path[last - path] = 0;
                uint32_t tmp_cl = 0;
                fat32_resolve_path(current_disk, parent_path, &tmp_cl);
                parent_cluster = tmp_cl;
            }
            // Read parent directory
            int n = fat32_read_dir(current_disk, parent_cluster, entries, 32);
            int found = 0;
            for (int i = 0; i < n; i++) {
                int match = 1;
                for (int k = 0; k < 11; k++) {
                    if (fatname[k] != entries[i].name[k]) { match = 0; break; }
                }
                if (match) {
                    found = 1;
                    uint32_t cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                    if ((entries[i].attr & 0x10) == 0x10) {
                        // Directory
                        if (recursive) {
                            rm_recursive(current_disk, cl);
                            // Remove directory itself
                            uint8_t sector[512];
                            uint32_t lba = fat32_cluster_to_lba(parent_cluster);
                            if (ata_read_sector(current_disk, lba, sector) == 0) {
                                for (int off = 0; off < 512; off += 32) {
                                    if (memcmp(&sector[off], entries[i].name, 11) == 0) {
                                        sector[off] = 0xE5;
                                        ata_write_sector(current_disk, lba, sector);
                                        break;
                                    }
                                }
                            }
                        } else {
                            kprint("Use rm -r to remove directory\n");
                        }
                    } else {
                        // File
                        uint8_t sector[512];
                        uint32_t lba = fat32_cluster_to_lba(parent_cluster);
                        if (ata_read_sector(current_disk, lba, sector) == 0) {
                            for (int off = 0; off < 512; off += 32) {
                                if (memcmp(&sector[off], entries[i].name, 11) == 0) {
                                    sector[off] = 0xE5;
                                    ata_write_sector(current_disk, lba, sector);
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
            }
            if (!found) {
                kprint("Not found\n");
            }
            return;
        }
        else if (strcmp(arg[0], "cd") == 0)
        {
            if (count < 2) {
                kprint("Usage: cd <path>\n");
                return;
            }
            int cd_res = fat32_change_dir(current_disk, arg[1]);
            if (cd_res == 0) {
                return;
            } else if (cd_res == -1) {
                kprintf("No such directory: %s\n", arg[1]);
            } else if (cd_res == -2) {
                kprintf("Not a directory: %s\n", arg[1]);
            } else if (cd_res == -3) {
                kprintf("Already at root directory\n");
            } else {
                kprintf("Failed to change directory\n");
            }
            return;
        }
        else if (strcmp(arg[0], "edit") == 0) {
            if (count < 2) {
                kprint("Usage: edit <filename>\n");
                return;
            }
            editor_main(arg[1]);
            return;
        }
        else if (strcmp(arg[0], "reboot") == 0)
        {
            reboot_system();
            return;
        }
        else if (strcmp(arg[0], "shutdown") == 0)
        {
            shutdown_system();
            return;
        }
        else if (strcmp(arg[0], "sleep") == 0)
        {
            if (count < 2) {
                kprint("Usage: sleep [milliseconds]\n");
                return;
            }
            int ms = atoi(arg[1]);
            if (ms <= 0) {
                kprint("Error: milliseconds must be positive\n");
                return;
            }
            pit_sleep(ms);
            return;
        }
        else if (strcmp(arg[0], "echo") == 0)
        {
            if (count < 2) {
                kprint("Usage: echo [message]\n");
                return;
            }
            
            // Собираем все аргументы в одну строку
            char message[256] = {0};
            int pos = 0;
            
            for (int i = 1; i < count; i++) {
                // Копируем аргумент
                int j = 0;
                while (arg[i][j] && pos < 255) {
                    message[pos++] = arg[i][j++];
                }
                // Добавляем пробел между аргументами
                if (i < count - 1 && pos < 255) {
                    message[pos++] = ' ';
                }
            }
            message[pos] = '\n';
            message[pos + 1] = 0;
            
            kprint(message);
            return;
        }
        else if (strcmp(arg[0], "pause") == 0)
        {
            kprint("Press any key to continue...\n");
            kgetch();
            return;
        }
        else if (strcmp(arg[0], "fcsasm") == 0) {
            if (count < 3) {
                kprintf("Usage: fcsasm <src.asm> <dst.ex>\n");
            } else {
                kprintf("FCSASM v0.0.4\n\n");
                fcsasm_compile(arg[1], arg[2]);
                kprint("\n");
            }
        }
        else
        {
            // Попытка запустить любой файл как бинарный
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            char fatname[12];
            memset(fatname, ' ', 11);
            fatname[11] = 0;
            int clen = strlen(arg[0]);
            int dot = -1;
            for (int i = 0; i < clen; i++) if (arg[0][i] == '.') { dot = i; break; }
            if (dot == -1) {
                for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(arg[0][i]);
            } else {
                for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(arg[0][i]);
                for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(arg[0][i]);
            }
            int found = 0;
            for (int i = 0; i < n; i++) {
                if (strncmp(entries[i].name, fatname, 11) == 0 && (entries[i].attr & 0x10) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                load_and_run_binary(arg[0], current_disk, current_dir_cluster);
                return;
            }

            if (startsWith(arg[0], "#"));
            else
            {
                kprintf("%s: command or executable file not found\n", arg[0]);
            }
        }
    }
}

// Helper function to convert hex character to number
uint8_t hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF; // Error
}

// Вспомогательная функция для поиска свободного кластера FAT32
uint32_t find_free_cluster(uint8_t drive) {
    for (uint32_t cl = 2; cl < 0x0FFFFFEF; cl++) {
        uint32_t next = fat32_get_next_cluster(drive, cl);
        if (next == 0x00000000) return cl;
    }
    return 0;
}

// Рекурсивное удаление директории по кластеру
void rm_recursive(uint8_t drive, uint32_t cluster) {
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(drive, cluster, entries, 32);
    for (int i = 0; i < n; i++) {
        if (entries[i].name[0] == 0xE5) continue;
        if ((entries[i].attr & 0x0F) == 0x08) continue; // volume label
        char name[12];
        int pos = 0;
        for (int j = 0; j < 8; j++) if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) name[pos++] = entries[i].name[j];
        int has_ext = 0;
        for (int j = 8; j < 11; j++) if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) has_ext = 1;
        if (has_ext) {
            name[pos++] = '.';
            for (int j = 8; j < 11; j++) if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) name[pos++] = entries[i].name[j];
        }
        name[pos] = 0;
        uint32_t cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
        if ((entries[i].attr & 0x10) == 0x10) {
            // Папка — рекурсивно
            rm_recursive(drive, cl);
        }
        // Удаляем саму запись
        // Для этого вызываем rm по имени (в текущем кластере)
        char pathbuf[256];
        strcpy(pathbuf, name);
        // Удаляем запись в текущем каталоге
        fat32_dir_entry_t del_entries[32];
        int del_n = fat32_read_dir(drive, cluster, del_entries, 32);
        uint8_t sector[512];
        uint32_t lba = fat32_cluster_to_lba(cluster);
        if (ata_read_sector(drive, lba, sector) == 0) {
            for (int off = 0; off < 512; off += 32) {
                if (memcmp(&sector[off], entries[i].name, 11) == 0) {
                    sector[off] = 0xE5;
                    ata_write_sector(drive, lba, sector);
                    break;
                }
            }
        }
    }
}
