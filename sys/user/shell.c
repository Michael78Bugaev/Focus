#include <stdint.h>
#include <kernel.h>
#include <mem.h>
#include <ata.h>

int current_disk = 0;
uint8_t * ide_buf; // Buffer for read/write operations

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
            kprint("read (-l lenght) [lba] - read sector\n");
            kprint("write [lba] [-h |-t] [data] - write data\n");
            kprint("bootsec - create boot sector\n");
            kprint("list - list available disks\n");
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