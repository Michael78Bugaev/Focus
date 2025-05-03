#include <stdint.h>
#include <kernel.h>
#include <mem.h>
#include <string.h>
#include <ata.h>
#include <fat32.h>

int current_disk = 0;
uint8_t * ide_buf; // Buffer for read/write operations

// Прототипы функций
uint8_t hex_to_int(char c);

// Локальная функция для перевода символа в верхний регистр
static char my_toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

void shell_execute(char *input)
{
    int count;
    char **arg = splitString(input, &count);

    if (count > 0)
    {
        if (strcmp(arg[0], "help") == 0)
        {
            kprint("Available commands:\n");
            kprint("help - show this help\n");
            kprint("clear - clear screen\n");
            kprint("disk [n] - select disk (0-3)\n");
            kprint("read (-l lenght) [lba] - read sector\n");
            kprint("write [lba] [-h |-t] [data] - write data\n");
            kprint("bootsec - create boot sector\n");
            kprint("list - list available disks\n");
            kprint("fatmount - mount FAT32 partition\n");
            kprint("fatls - list files in FAT32 root directory\n");
            kprint("fatcat - display file content\n");
            kprint("fatmkfs - create FAT32 filesystem\n");
            kprint("fatinfo - display FAT32 volume label and filesystem type\n");
            kprint("fattouch - create empty file\n");
            kprint("fatmkdir - create directory\n");
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
            
            // Проверка параметров
            if (num_sectors <= 0 || num_sectors > 256) {
                kprint("Error: sectors must be between 1 and 256\n");
                return;
            }
            
            if (lba < 0) {
                kprint("Error: LBA must be non-negative\n");
                return;
            }
            
            // Конвертируем hex строку в данные
            int data_len = strlen(hex_data);
            if (data_len % 2 != 0) {
                kprint("Error: hex data must have even length\n");
                return;
            }
            
            // Очищаем буфер
            for (int i = 0; i < 512; i++) {
                ide_buf[i] = 0;
            }
            
            // Конвертируем hex в данные
            for (int i = 0; i < data_len; i += 2) {
                uint8_t high = hex_to_int(hex_data[i]);
                uint8_t low = hex_to_int(hex_data[i + 1]);
                if (high == 0xFF || low == 0xFF) {
                    kprintf("Error: invalid hex character at position %d\n", i);
                    return;
                }
                ide_buf[i/2] = (high << 4) | low;
            }

            // Записываем сектора
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
            // Проверка на флаг -l
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
            // Проверка параметров
            if (num_sectors <= 0 || num_sectors > 256) {
                kprint("Error: sectors must be between 1 and 256\n");
                return;
            }
            if (lba < 0) {
                kprint("Error: LBA must be non-negative\n");
                return;
            }
            
            // Читаем сектора
            for (int i = 0; i < num_sectors; i++) {
                if (ata_read_sector(current_disk, lba + i, ide_buf) != 0) {
                    kprintf("Error reading sector %d\n", lba + i);
                    return;
                }
                
                // Выводим данные в hex+ascii стиле (16 байт на строку)
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
            // Удаляем пробелы справа
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
        else if (strcmp(arg[0], "fatls") == 0)
        {
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, 2, entries, 32); // 2 - корневой кластер FAT32
            if (n < 0) {
                kprint("Error read directory\n");
                return;
            }
            for (int i = 0; i < n; i++) {
                char name[13];
                int pos = 0;
                // Имя (8 символов)
                for (int j = 0; j < 8; j++) {
                    if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) name[pos++] = entries[i].name[j];
                }
                // Расширение (3 символа)
                int has_ext = 0;
                for (int j = 8; j < 11; j++) {
                    if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) has_ext = 1;
                }
                if (has_ext) {
                    name[pos++] = '.';
                    for (int j = 8; j < 11; j++) {
                        if (entries[i].name[j] != ' ' && entries[i].name[j] != 0) name[pos++] = entries[i].name[j];
                    }
                }
                name[pos] = 0;
                if (name[0] == 0) strcpy(name, "(NO NAME)");
                // Определяем тип: папка, файл или пропустить
                if ((entries[i].attr & 0x0F) == 0x08) continue; // volume label, skip
                kprintf("ATTR=%02X ", entries[i].attr);
                if ((entries[i].attr & 0x10) == 0x10) {
                    kprintf("<DIR>  %s\n", name);
                } else {
                    kprintf("<FILE> %s\n", name);
                }
            }
            return;
        }
        else if (strcmp(arg[0], "fatcat") == 0)
        {
            if (count < 2) {
                kprint("Usage: fatcat [FILENAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, 2, entries, 32);
            if (n < 0) {
                kprint("Error read directory\n");
                return;
            }
            char *filename = arg[1];
            for (int i = 0; i < n; i++) {
                char name[12];
                strncpy(name, entries[i].name, 11);
                name[11] = 0;
                for (int j = 10; j >= 0; j--) {
                    if (name[j] == ' ') name[j] = 0;
                    else break;
                }
                if (strcmp(name, filename) == 0) {
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
            // Минимальный x86 bootloader (BIOS, 16 бит)
            const uint8_t bootloader_bin[62] = {
                0xEB, 0x3C, 0x90, // jmp short 0x3E
                // OEM и BPB будут заполнены ниже
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
            // Вставляем bootloader
            strncpy(sector, bootloader_bin, sizeof(bootloader_bin));
            strncpy(sector + 0x4E, bootmsg, sizeof(bootmsg));
            // BPB и метки (как раньше)
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
            *(uint32_t*)&sector[32] = 65536; // total sectors 32 (пример: 32 МБ)
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
            // Запись Boot Sector
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
            // Очищаем FAT и корневой кластер
            memset(sector, 0, 512);
            for (int i = 0; i < 32; i++) {
                ata_write_sector(current_disk, 32 + i, sector); // корневой каталог
            }
            for (int i = 0; i < 123 * 2; i++) {
                ata_write_sector(current_disk, 32 + 32 + i, sector); // FAT
            }
            kprint("FAT32 created\n");
            return;
        }
        else if (strcmp(arg[0], "fattouch") == 0)
        {
            if (count < 2) {
                kprint("Usage: fattouch [FILENAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, 2, entries, 32);
            if (n < 0 || n >= 32) {
                kprint("Directory full or error\n");
                return;
            }
            // Имя файла (8.3, без точки)
            char name[11] = "        ";
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            if (arg[1][i] == '.') {
                i++;
                for (int k = 0; k < 3 && arg[1][i]; k++, i++) name[8 + k] = my_toupper(arg[1][i]);
            }
            // Найти свободную запись
            uint8_t sector[512];
            uint32_t lba = fat32_cluster_to_lba(2);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                kprint("Error reading root dir\n");
                return;
            }
            for (int off = 0; off < 512; off += 32) {
                if (sector[off] == 0x00 || sector[off] == 0xE5) {
                    memset(&sector[off], 0, 32);
                    strncpy(&sector[off], name, 11);
                    sector[off + 11] = 0x20; // attr: archive
                    if (ata_write_sector(current_disk, lba, sector) != 0) {
                        kprint("Error writing root dir\n");
                        return;
                    }
                    kprint("File created\n");
                    return;
                }
            }
            kprint("No free entry\n");
            return;
        }
        else if (strcmp(arg[0], "fatmkdir") == 0)
        {
            if (count < 2) {
                kprint("Usage: fatmkdir [DIRNAME]\n");
                return;
            }
            fat32_dir_entry_t entries[32];
            int n = fat32_read_dir(current_disk, 2, entries, 32);
            if (n < 0 || n >= 32) {
                kprint("Directory full or error\n");
                return;
            }
            // Имя папки (8.3, без точки)
            char name[11] = "        ";
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            // Найти свободную запись
            uint8_t sector[512];
            uint32_t lba = fat32_cluster_to_lba(2);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                kprint("Error reading root dir\n");
                return;
            }
            for (int off = 0; off < 512; off += 32) {
                if (sector[off] == 0x00 || sector[off] == 0xE5) {
                    memset(&sector[off], 0, 32);
                    strncpy(&sector[off], name, 11);
                    sector[off + 11] = 0x10; // attr: directory (только папка, без других битов)
                    if (ata_write_sector(current_disk, lba, sector) != 0) {
                        kprint("Error writing root dir\n");
                        return;
                    }
                    kprint("Directory created\n");
                    return;
                }
            }
            kprint("No free entry\n");
            return;
        }
        else
        {
            kprintf("Wrong command\n");
        }
    }
}

// Вспомогательная функция для конвертации hex символа в число
uint8_t hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF; // Ошибка
}