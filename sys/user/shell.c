#include <stdint.h>
#include <kernel.h>
#include <fat32.h>
#include <mem.h>
#include <ata.h>

int current_disk = 0;
uint16_t ide_buf[256]; // Buffer for read/write operations

// Прототипы функций
uint8_t hex_to_int(char c);

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
            kprint("read [sectors] [lba] - read sectors\n");
            kprint("write [data] [sectors] [lba] - write data\n");
            kprint("bootsec - create boot sector\n");
            kprint("mkfat32 - create FAT32 partition\n");
            kprint("list - list available disks\n");
            kprint("\nFile system commands:\n");
            kprint("ls [path] - list directory contents\n");
            kprint("cd [path] - change directory\n");
            kprint("pwd - print working directory\n");
            kprint("mkdir [path] - create directory\n");
            kprint("touch [path] - create empty file\n");
            kprint("rm [path] - delete file or directory\n");
            kprint("mv [source] [dest] - move/rename file or directory\n");
            return;
        }
        else if (strcmp(arg[0], "clear") == 0)
        {
            kclear();
            return;
        }
        else if (strcmp(arg[0], "mkfat32") == 0)
        {
            fat32_init(current_disk);
            return;
        }
        else if (strcmp(arg[0], "format32") == 0)
        {
            kprint("Enter volume label: ");
            char label[12];
            get_string(label);
            fat32_format(current_disk, label);
            return;
        }
        else if (strcmp(arg[0], "list") == 0)
        {
            int count = ata_get_device_count();
            kprintf("Found %d ATA devices:\n", count);
            for (int i = 0; i < count; i++) {
                kprintf("%d: %s (Size: %d MB)\n", 
                    i, 
                    ata_get_device_name(i),
                    ata_get_device_size(i) / (1024 * 1024)
                );
            }
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

            if (ata_write_sectors(current_disk, 0, 1, (uint16_t*)bootsec)) {
                kprint("Boot sector written\n");
            } else {
                kprint("Failed to write boot sector\n");
            }
            return;
        }
        else if (strcmp(arg[0], "disk") == 0)
        {
            if (count > 1)
            {
                int new_disk = atoi(arg[1]);
                if (new_disk >= 0 && new_disk < ata_get_device_count())
                {
                    current_disk = new_disk;
                    kprintf("Selected disk %d: %s\n", current_disk, ata_get_device_name(current_disk));
                }
                else
                {
                    kprint("Invalid disk number\n");
                }
            }
            else
            {
                kprintf("Current disk: %d: %s\n", current_disk, ata_get_device_name(current_disk));
            }
            return;
        }
        else if (strcmp(arg[0], "mkfat32") == 0)
        {
            kprintf("Creating FAT32 on disk %d: %s\n", current_disk, ata_get_device_name(current_disk));
            kprintf("Enter volume label: ");
            char label[12];
            get_string(label);
            
            if (fat32_format(current_disk, label)) {
                kprint("Disk formatted successfully\n");
                if (fat32_init(current_disk)) {
                    kprint("FAT32 initialized\n");
                } else {
                    kprint("Failed to initialize FAT32\n");
                }
            } else {
                kprint("Failed to format disk\n");
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
            for (int i = 0; i < 256; i++) {
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
            
            kprintf("Writing to disk %d: %s\n", current_disk, ata_get_device_name(current_disk));
            kprintf("LBA: %d, Sectors: %d\n", lba, num_sectors);
            
            if (ata_write_sectors(current_disk, lba, num_sectors, ide_buf)) {
                kprintf("Successfully wrote %d sectors at LBA %d\n", num_sectors, lba);
            } else {
                kprint("Failed to write data\n");
            }
            return;
        }
        else if (strcmp(arg[0], "read") == 0)
        {
            if (count < 3)
            {
                kprint("Usage: read [sectors] [lba]\n");
                kprint("  sectors - number of sectors to read\n");
                kprint("  lba     - starting sector number\n");
                return;
            }
            
            int num_sectors = atoi(arg[1]);
            int lba = atoi(arg[2]);
            
            // Проверка параметров
            if (num_sectors <= 0 || num_sectors > 256) {
                kprint("Error: sectors must be between 1 and 256\n");
                return;
            }
            
            if (lba < 0) {
                kprint("Error: LBA must be non-negative\n");
                return;
            }
            
            kprintf("Reading from disk %d: %s\n", current_disk, ata_get_device_name(current_disk));
            kprintf("LBA: %d, Sectors: %d\n", lba, num_sectors);
            
            if (ata_read_sectors(current_disk, lba, num_sectors, ide_buf)) {
                // Выводим данные в формате hex dump
                for (int i = 0; i < num_sectors * 256; i++) {
                    if (i % 16 == 0) {
                        if (i > 0) {
                            // Вывод ASCII представления
                            kprint("  ");
                            for (int j = i - 16; j < i; j++) {
                                uint8_t byte = ide_buf[j] & 0xFF;
                                if (byte >= 32 && byte <= 126) {
                                    kputchar(byte, 0x07);
                                } else {
                                    kputchar('.', 0x07);
                                }
                            }
                        }
                        kprintf("\n%04X: ", i);
                    }
                    kprintf("%02X ", ide_buf[i] & 0xFF);
                }
                // Вывод ASCII для последней строки
                int remaining = (num_sectors * 256) % 16;
                if (remaining == 0) remaining = 16;
                for (int i = 0; i < (16 - remaining); i++) {
                    kprint("   ");
                }
                kprint("  ");
                for (int i = num_sectors * 256 - remaining; i < num_sectors * 256; i++) {
                    uint8_t byte = ide_buf[i] & 0xFF;
                    if (byte >= 32 && byte <= 126) {
                        kputchar(byte, 0x07);
                    } else {
                        kputchar('.', 0x07);
                    }
                }
                kprintf("\nSuccessfully read %d sectors from LBA %d\n", num_sectors, lba);
            } else {
                kprint("Failed to read data\n");
            }
            return;
        }
        else if (strcmp(arg[0], "ls") == 0)
        {
            char path[FAT32_MAX_PATH];
            if (count > 1) {
                strncpy(path, arg[1], FAT32_MAX_PATH);
            } else {
                path[0] = '\0';
            }
            
            if (!fat32_list_directory(path)) {
                kprint("Failed to list directory\n");
            }
            return;
        }
        else if (strcmp(arg[0], "cd") == 0)
        {
            if (count < 2) {
                kprint("Usage: cd [path]\n");
                return;
            }
            
            if (!fat32_change_directory(arg[1])) {
                kprintf("Failed to change directory to %s\n", arg[1]);
            } else {
                char current_dir[FAT32_MAX_PATH];
                fat32_get_current_directory(current_dir, FAT32_MAX_PATH);
                kprintf("Current directory: %s\n", current_dir);
            }
            return;
        }
        else if (strcmp(arg[0], "pwd") == 0)
        {
            char current_dir[FAT32_MAX_PATH];
            if (fat32_get_current_directory(current_dir, FAT32_MAX_PATH)) {
                kprintf("%s\n", current_dir);
            } else {
                kprint("Failed to get current directory\n");
            }
            return;
        }
        else if (strcmp(arg[0], "mkdir") == 0)
        {
            if (count < 2) {
                kprint("Usage: mkdir [path]\n");
                return;
            }
            
            if (!fat32_create_directory(arg[1])) {
                kprintf("Failed to create directory %s\n", arg[1]);
            } else {
                kprintf("Directory %s created\n", arg[1]);
            }
            return;
        }
        else if (strcmp(arg[0], "rm") == 0)
        {
            if (count < 2) {
                kprint("Usage: rm [path]\n");
                return;
            }
            
            if (!fat32_delete(arg[1])) {
                kprintf("Failed to delete %s\n", arg[1]);
            } else {
                kprintf("%s deleted\n", arg[1]);
            }
            return;
        }
        else if (strcmp(arg[0], "mv") == 0)
        {
            if (count < 3) {
                kprint("Usage: mv [source] [destination]\n");
                return;
            }
            
            if (!fat32_rename(arg[1], arg[2])) {
                kprintf("Failed to move/rename %s to %s\n", arg[1], arg[2]);
            } else {
                kprintf("Moved/renamed %s to %s\n", arg[1], arg[2]);
            }
            return;
        }
        else if (strcmp(arg[0], "touch") == 0)
        {
            if (count < 2) {
                kprint("Usage: touch [path]\n");
                return;
            }
            
            if (!fat32_create_file(arg[1])) {
                kprintf("Failed to create file %s\n", arg[1]);
            } else {
                kprintf("File %s created\n", arg[1]);
            }
            return;
        }
        else if (strcmp(arg[0], "fat_ls") == 0)
        {
            
        }
        else if (strcmp(arg[0], "ye") == 0)
        {
            kprint("\nBeautiful morning, you're the sun in my morning, babe\nNothing unwanted\nBeautiful morning, you're the sun in my morning, babe\nNothing unwanted\n\nI just want to feel liberated, I, I, na-na-na\nI just want to feel liberated, I, I, na-na-na\nIf I ever instigated, I'm sorry\nTell me, who in here can relate? I, I, I\n\n");
        }
        else
        {
            kprintf("Unknown command: %s\n", arg[0]);
        }
    }
    return;
}

// Вспомогательная функция для конвертации hex символа в число
uint8_t hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF; // Ошибка
}