#include <stdint.h>
#include <mem.h>
#include <string.h>
#include <ata.h>
#include <fat32.h>
#include <fcsasm.h>
#include <fcs_vm.h>
#include "elf.h"
#ifndef ET_EXEC
#define ET_EXEC 2
#endif
#include <iso9660.h>
#include <sysinfo.h>
#include <net_if.h>
#include <stddef.h>
#include "elf_loader.h"
#include <paging.h>

#define USER_STACK_SIZE  0x00010000u /* 64 KiB */
/*
 * Place the user-mode stack just above the new user programme area that now
 * starts at 0x50000000 (see user.ld).  This keeps both the code and its stack
 * well clear of the kernel dynamic memory pool.
 */
#define USER_STACK_TOP   0x5DBFF000u /* Top address (exclusive) – just below 1500 MiB of RAM */
#define USER_STACK_BASE  (USER_STACK_TOP - USER_STACK_SIZE)

extern void snake_main();

static int run_fex_fat(const char *path);
static int run_fex_iso(const char *path);

static void str_append(char *dest, const char *src, size_t buf_size);

// Прототипы функций
static int isocpy_file(const char* src, const char* dst);
static int isocpy_dir(const char* src, const char* dst);

static inline uint16_t htons(uint16_t x) { return (x>>8) | (x<<8); }
extern void net_poll_once(void);
extern int net_dequeue_frame(uint8_t **data, uint16_t *len);

static void fat_name_from_string(const char *src, char dest[11]);

int current_disk = 0;
static uint8_t ide_buf[512]; // Buffer for read/write operations

// Path prefix where the shell will search for binary (.FEX) executables when
// a command is not recognised.  Can be changed at run-time by entering the
// assignment  BIN=<path>  (for example  BIN=CDROM:/focus/bin/  or  BIN=0:\BIN\ ).
// Empty string means "use current directory only".
static char bin_path[256] = "";
static char drv_path[128] = "";
extern fat32_bpb_t fat32_bpb;
// Prototypes of functions
uint8_t hex_to_int(char c);
uint32_t find_free_cluster(uint8_t drive);

extern int build_path(uint32_t cluster, char path[][9], int max_depth);
static int ensure_fat32_path(uint8_t disk, const char* path, uint32_t* out_dir_cluster, char* out_filename);

static int cmpiso_file(const char* src, const char* dst);

// Local function to convert character to uppercase
static char my_toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

// Search for name and parent by cluster: returns 1 if found, 0 if not
static int find_name_and_parent(uint32_t search_cluster, uint32_t current_cluster, char* out_name, uint32_t* out_parent) {
    fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
    if (!entries) {
        kprintf("<(0c)>find_name_and_parent: out of mem<(0f)>\n");
        return 0;
    }
    int n = fat32_read_dir(0, current_cluster, entries, 32);
    for (int i = 0; i < n; i++) {
        if ((entries[i].attr & 0x10) == 0x10) {
            /* Пропускаем специальные записи "." и ".." – иначе возможна бесконечная рекурсия */
            if (entries[i].name[0] == '.') {
                continue;
            }
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
                mfree(entries);
                return 1;
            }
            if (find_name_and_parent(search_cluster, cl, out_name, out_parent)) {
                mfree(entries);
                return 1;
            }
        }
    }
    mfree(entries);
    return 0;
}

// Build path up by parents
int build_path(uint32_t cluster, char path[][9], int max_depth) {
    extern uint32_t root_dir_first_cluster;
    int depth = 0;
    uint32_t cur = cluster;

    while (cur != root_dir_first_cluster && depth < max_depth) {
        /* 1. Прочитаем текущий каталог, чтобы узнать кластер родителя (запись "..") */
        fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
        if (!entries) break;
        int n = fat32_read_dir(0, cur, entries, 32);
        uint32_t parent_cluster = root_dir_first_cluster;
        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] == '.' && entries[i].name[1] == '.') {
                parent_cluster = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                if (parent_cluster == 0) parent_cluster = root_dir_first_cluster;
                break;
            }
        }

        /* 2. Прочитаем родительский каталог, чтобы найти имя текущего каталога */
        fat32_dir_entry_t *pentries = malloc(sizeof(fat32_dir_entry_t) * 32);
        if (!pentries) { mfree(entries); break; }
        int pn = fat32_read_dir(0, parent_cluster, pentries, 32);
        char name[9] = {0};
        for (int i = 0; i < pn; i++) {
            if ((pentries[i].attr & 0x10) != 0x10) continue; /* только каталоги */
            uint32_t cl = ((uint32_t)pentries[i].first_cluster_high << 16) | pentries[i].first_cluster_low;
            if (cl == cur) {
                int pos = 0;
                for (int j = 0; j < 8; j++) {
                    if (pentries[i].name[j] != ' ' && pentries[i].name[j] != 0) {
                        name[pos++] = pentries[i].name[j];
                    }
                }
                name[pos] = 0;
                break;
            }
        }

        /* 3. Сохраняем имя в массиве path */
        for (int i = 0; i < 9; i++) path[depth][i] = name[i];

        /* 4. Освобождаем память и поднимаемся вверх */
        mfree(entries);
        mfree(pentries);
        cur = parent_cluster;
        depth++;
    }
    return depth;
}

// Выполнить .fsc файл как скрипт (поддержка cdrom:/ и абсолютных путей FAT32)
void shell_execute_fsc(const char* fname) {
    // ISO9660 script
    if (strncmp(fname, "cdrom:/", 7) == 0) {
        const char* iso_path = fname + 7;
        char* buf = malloc(4096);
        if (!buf) { kprintf("<(04)>Error allocating memory!<(0f)>\n"); return; }
        int sz = iso9660_read(iso_path, buf, 4095);
        if (sz <= 0) { kprintf("<(04)>Cannot read script: %s. sz: %d<(0f)>\n", fname, sz); mfree(buf); return; }
        buf[sz] = 0;
        char* line = strtok(buf, "\n");
        while (line) {
            while (*line == ' ' || *line == '\t') line++;
            if (*line && *line != '#') shell_execute(line);
            line = strtok(NULL, "\n");
        }
        mfree(buf);
        return;
    }
    // FAT32 script (relative or absolute)
    uint32_t dir_cluster = current_dir_cluster;
    char filename_buf[13];
    if (fname[1] == ':' && (fname[2] == '\\' || fname[2] == '/')) {
        if (ensure_fat32_path(current_disk, fname, &dir_cluster, filename_buf) != 0) {
            kprintf("Cannot read script: %s\n", fname);
            return;
        }
    } else {
        strncpy(filename_buf, fname, 12);
        filename_buf[12] = 0;
    }
    fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
    if (!entries)
    {
        kprintf("<(0c)>Error memory allocation for fat32 dir entries.\n");
        return;
    }
    int n = fat32_read_dir(current_disk, dir_cluster, entries, 32);
    char fatname[12];
    memset(fatname, ' ', 11);
    fatname[11] = 0;
    int clen = strlen(filename_buf);
    int dot = -1;
    for (int i = 0; i < clen; i++) if (filename_buf[i] == '.') { dot = i; break; }
    if (dot == -1) {
        for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(filename_buf[i]);
    } else {
        for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(filename_buf[i]);
        for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(filename_buf[i]);
    }
    int found = 0; uint32_t cl = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i].name, fatname, 11) == 0) {
            cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            found = 1; break;
        }
    }
    if (!found) { kprintf("Cannot read script: %s\n", fname); return; }
    char *buffer = malloc(4096);
    if (!buffer) { kprintf("Error allocating memory\n"); return; }
    int size = fat32_read_file(current_disk, cl, (uint8_t*)buffer, 4095);
    if (size <= 0) { kprintf("Cannot read script: %s\n", fname); mfree(buffer); return; }
    buffer[size] = 0;
    char* line = strtok(buffer, "\n");
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line && *line != '#') shell_execute(line);
        line = strtok(NULL, "\n");
    }
    mfree(buffer);
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
            kprint("    xxd <filename> - display hex dump of a file\n");
            kprint("    isomount <devnum> - mount ISO9660 volume\n");
            kprint("    isols - list files in ISO9660 volume\n");
            kprint("    sysinfo - display system information\n");
            kprint("    ping <ip> - send ICMP echo requests\n");
            return;
        }
        else if (strcmp(arg[0], "clear") == 0)
        {
            kclear();
            return;
        }
        else if (strcmp(arg[0], "whatsnew") == 0)
        {
            /* Read and print the 'whatsnew' file like a script */
            char *buf = malloc(4096);
            if (!buf) { kprintf("<(04)>Error allocating memory!<(0f)>\n"); return; }
            /* Read from ISO root directory "focus" */
            int sz = iso9660_read("CDROM:/focus/whn.txt", buf, 4095);
            if (sz <= 0) {
                kprintf("<(0c)>Cannot read whatsnew file (sz=%d)!<(0f)>\n", sz);
                mfree(buf);
                return;
            }
            buf[sz] = '\0';
            /* Split into lines and print each */
            char *line = strtok(buf, "\n");
            while (line) {
                /* Trim leading whitespace */
                while (*line == ' ' || *line == '\t') line++;
                /* Strip trailing CR */
                char *end = line + strlen(line) - 1;
                if (end >= line && *end == '\r') *end = '\0';
                if (*line && *line != '#') {
                    kprintf(line);
                }
                line = strtok(NULL, "\n");
            }
            mfree(buf);
            return;
        }
        else if (strcmp(arg[0], "halt") == 0)
        {
            for (;;);
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
            fat32_dir_entry_t *entries = malloc(32 * sizeof(fat32_dir_entry_t));
            if (!entries) {
                kprintf("<(0c)>Error allocating memory<(0f)>\n");
                qemu_debug_printf("Error allocating memory\n");
                return;
            }
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0) {
                kprint("<(0c)>Error reading directory<(0f)>\n");
                mfree(entries);
                qemu_debug_printf("Error reading directory\n");
                return;
            }
            mfree(entries);
            qemu_debug_printf("n: %d\n", n);
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
                    fat32_dir_entry_t *subentries = malloc(32 * sizeof(fat32_dir_entry_t));
                    if (!subentries) {
                        kprint("<(0c)>Error allocating memory for fat struct entries<(0f)>\n");
                        return;
                    }
                    int subn = fat32_read_dir(current_disk, 
                        ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low, 
                        subentries, 32);
                    if (subn < 0) {
                        kprint("<(0c)>Error reading directory<(0f)>\n");
                        mfree(subentries);
                        return;
                    }
                    mfree(subentries);
                    
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
                kprint("<(0c)>Usage: cat [FILENAME]<(0f)>\n");
                return;
            }
            fat32_dir_entry_t *entries = malloc(32 * sizeof(fat32_dir_entry_t));
            if (!entries) {
                kprint("<(0c)>Error allocating memory for fat struct entries<(0f)>\n");
                return;
            }
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0) {
                kprint("<(0c)>Error read directory<(0f)>\n");
                mfree(entries);
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
                    uint32_t to_read = (size > 1024) ? 1024 : size;
                    uint8_t *buf = malloc(to_read);
                    if (!buf) { kprintf("cat: OOM\n"); mfree(entries); return; }
                    int read = fat32_read_file(current_disk, cluster, buf, to_read);
                    for (int b = 0; b < read; b++) {
                        kputchar(buf[b], 0x07);
                    }
                    kprint("\n");
                    mfree(buf);
                    mfree(entries);
                    return;
                }
            }
            kprint("<(0c)>cat: %s: File not found<(0f)>\n", arg[1]);
            mfree(entries);
            return;
        }
        else if (strcmp(arg[0], "fatmkfs") == 0)
        {
            // Minimal x86 bootloader (BIOS, 16 bit)
            uint8_t bootloader_bin_[62] = {
                0xEB, 0x21, 0x90,             /* jmp short start (to 0x21) */
                /* BPB (заполняется ниже) */
                /* offset 0x21 (start): */
                0xB8, 0x00, 0x7C,             /* mov ax,0x7C00 */
                0x8E, 0xD8,                   /* mov ds,ax */
                0xBE, 0x4E, 0x00,             /* mov si,0x4E */
                /* loop: */
                0xAC,                         /* lodsb */
                0x0C, 0x00,                   /* or al,al */
                0x74, 0x09,                   /* jz hang */
                0xB4, 0x0E,                   /* mov ah,0x0E */
                0xBB, 0x07, 0x00,             /* mov bx,0x0007 */
                0xCD, 0x10,                   /* int 0x10 */
                0xEB, 0xF3,                   /* jmp short loop */
                /* hang: */
                0xF4,                         /* hlt */
                0xEB, 0xFE,                   /* jmp $ */
                /* padding до 62 байт */
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
            };
            const char bootmsg[] = "This is not a bootable disk\r\n";
            uint8_t* sector = malloc(512);
            if (!sector) {
                kprintf("<(0c)>Error allocating memory<(0f)>\n");
                return;
            }
            memset(sector, 0, 512);
            // 1. Jump (3 байта)
            sector[0] = 0xEB; sector[1] = 0x3C; sector[2] = 0x90;
            // 2. BPB (с 3 по 0x3A)
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
            // 3. Код загрузчика (начиная с 0x3E)
            for (int i = 0; i < (int)(sizeof(bootloader_bin_) - 3); i++)
                sector[0x3E + i] = bootloader_bin_[3 + i];
            // 4. Сообщение (начиная с 0x4E)
            for (int i = 0; i < (int)sizeof(bootmsg); i++)
                sector[0x4E + i] = bootmsg[i];
            sector[510] = 0x55; sector[511] = 0xAA;
            // Write Boot Sector
            if (ata_write_sector(current_disk, 0, sector) != 0) {
                kprintf("<(0c)>Error writing Boot Sector<(0f)>\n");
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
                kprintf("<(0c)>Error writing FSInfo<(0f)>\n");
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
                kprintf("<(0c)>Usage: touch [FILENAME]<(0f)>\n");
                return;
            }
            fat32_dir_entry_t *entries = malloc(32 * sizeof(fat32_dir_entry_t));
            if (!entries) {
                kprintf("<(0c)>Error allocating memory for fat struct entries<(0f)>\n");
                return;
            }
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0 || n >= 32) {
                kprintf("<(0c)>Directory full or error<(0f)>\n");
                mfree(entries);
                return;
            }
            // File name (8.3, without dot)
            char name[11];
            memset(name, ' ', 11);
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            /* Обрабатываем расширение (если было указано) */
            if (arg[1][i] == '.') {
                i++; int k = 0;
                while (arg[1][i] && k < 3) {
                    name[8 + k] = my_toupper(arg[1][i]);
                    i++; k++;
                }
            }

            /* --- Проверяем, что запись с таким именем ещё не существует --- */
            for (int idx = 0; idx < n; idx++) {
                if (entries[idx].name[0] == 0xE5 || entries[idx].name[0] == 0x00) continue; // удалённая/пустая
                if (memcmp(entries[idx].name, name, 11) == 0) {
                    kprintf("<(0c)>Entry with this name already exists<(0f)>\n");
                    mfree(entries);
                    return;
                }
            }
            // Find free entry
            uint8_t *sector = malloc(512);
            if (!sector) { kprintf("touch: OOM sector\n"); mfree(entries); return; }
            uint32_t lba = fat32_cluster_to_lba(current_dir_cluster);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                 kprintf("<(0c)>Error reading dir<(0f)>\n");
                 mfree(entries);
                 mfree(sector);
                 return;
             }
             for (int off = 0; off < 512; off += 32) {
                 if (sector[off] == 0x00 || sector[off] == 0xE5) {
                     // First erase entire sector
                     uint8_t *new_sector = malloc(512);
                     if (!new_sector) { kprintf("touch: OOM newsec\n"); mfree(entries); mfree(sector); return; }
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
                     new_sector[off + 11] = 0x20; // attr: file
                     // --- Выделяем свободный кластер и записываем его ---
                     uint32_t cl = find_free_cluster(current_disk);
                     if (cl == 0) {
                         kprintf("<(0c)>No free cluster<(0f)>\n");
                         mfree(entries);
                         return;
                     }
                     uint32_t fat_lba = fat_start + (cl * 4) / 512;
                     uint8_t *fat_sec = malloc(512);
                     if (!fat_sec) { kprintf("touch: OOM fatsec\n"); mfree(entries); mfree(sector); mfree(new_sector); return; }
                     if (ata_read_sector(current_disk, fat_lba, fat_sec) != 0) {
                         kprintf("<(0c)>Error reading FAT<(0f)>\n");
                         mfree(entries);
                         mfree(sector);
                         mfree(new_sector);
                         mfree(fat_sec);
                         return;
                     }
                     uint32_t fat_off = (cl * 4) % 512;
                     *(uint32_t*)&fat_sec[fat_off] = 0x0FFFFFFF;
                     if (ata_write_sector(current_disk, fat_lba, fat_sec) != 0) {
                         kprintf("<(0c)>Error writing FAT<(0f)>\n");
                         mfree(entries);
                         mfree(sector);
                         mfree(new_sector);
                         mfree(fat_sec);
                         return;
                     }
                     *(uint16_t*)(&new_sector[off + 20]) = (uint16_t)((cl >> 16) & 0xFFFF); // high
                     *(uint16_t*)(&new_sector[off + 26]) = (uint16_t)(cl & 0xFFFF); // low
                     // Write updated sector
                     if (ata_write_sector(current_disk, lba, new_sector) != 0) {
                         kprintf("<(0c)>Error writing dir<(0f)>\n");
                         mfree(entries);
                         mfree(sector);
                         mfree(new_sector);
                         mfree(fat_sec);
                         return;
                     }
                     mfree(fat_sec);
                     mfree(new_sector);
                     mfree(sector);
                     mfree(entries);
                     return;
                 }
             }
             kprintf("<(0c)>No free entry<(0f)>\n");
             mfree(sector);
             mfree(entries);
             return;
        }
        else if (strcmp(arg[0], "isotools") == 0)
        {
            kprintf("<(0a)>isotools v1.0 (Focus v1.5)<(0f)>\n");
            kprintf("Copyright 2025 Michael Bugaev\n");
            kprintf("License MIT: <(0b)>https://opensource.org/licenses/MIT<(0f)>\n");
            kprintf("This is free software: you are free to change and redistribute it.\n");
            kprintf("There is NO WARRANTY, to the extent permitted by law.\n");
            kprintf("Type 'help' for more information.\n");
            kprintf("Type 'exit' or 'quit' to exit.\n\n");
            int devnum = iso9660_atapi_devnum;
            char *iso_current_dir = malloc(512);
            if (!iso_current_dir) { kprintf("isotools: OOM\n"); return; }
            iso_current_dir[0] = 0; // initialize current directory
            kprintf("Current ATAPI CD-ROM: <(0e)>#%d<(0f)>\n\n", devnum);
            uint8_t *cmd = malloc(1024);
            if (!cmd) {
                kprintf("<(04)>Error allocating memory<(0f)>\n\n");
                return;
            }
            while (1) {
                kprintf("<(08)>[isotools (cdrom:/%s)]> ", iso_current_dir);
                get_string(cmd);
                // Разбиваем cmd на подкоманды
                int sub_count = 0;
                char **subarg = splitString(cmd, &sub_count);
                if (sub_count == 0) continue;
                if (strcmp(subarg[0], "exit") == 0) break;
                if (strcmp(subarg[0], "mount") == 0) {
                    int devnum = iso9660_atapi_devnum;
                    if (sub_count > 1) devnum = atoi(subarg[1]);
                    if (iso9660_mount_dev(devnum) == 0) {
                        kprintf("ISO9660 successfully mounted (ATAPI #%d)\n", devnum);
                    } else {
                        kprintf("<(0c)>Error mounting ISO9660<(0f)>\n\n");
                    }


                } 
                if (strcmp(subarg[0], "quit") == 0) break;
                else if (strcmp(subarg[0], "ls") == 0) {
                    uint8_t *sector = malloc(2048);
                    if (!sector) {
                        kprintf("<(0c)>Error allocating memory<(0f)>\n\n");
                        continue;
                    }
                    uint32_t dir_lba = g_root_dir_lba;
                    uint32_t dir_size = g_root_dir_size;
                    if (iso_current_dir[0]) {
                        if (iso9660_find(iso_current_dir, &dir_lba, &dir_size) != 0) {
                            kprintf("<(0c)>Error: directory not found: %s<(0f)>\n\n", iso_current_dir);
                            mfree(sector);
                            continue;
                        }
                    }
                    extern uint32_t g_root_dir_lba, g_root_dir_size;
                    if (atapi_read_device(iso9660_atapi_devnum, dir_lba, 1, sector) != 0) {
                        kprintf("<(0c)>Error reading ISO directory<(0f)>\n\n");
                        mfree(sector);
                        continue;
                    }
                    if (iso_current_dir[0])
                        kprintf("cdrom:/%s:\n", iso_current_dir);
                    else
                        kprintf("cdrom:/:\n");
                    size_t offset = 0;
                    while (offset < ISO9660_SECTOR_SIZE) {
                        uint8_t len = sector[offset];
                        if (len == 0) break;
                        uint8_t name_len = sector[offset+32];
                        char* name = (char*)&sector[offset+33];
                        if (!(name_len == 1 && (name[0] == 0 || name[0] == 1))) {
                            int real_len = 0;
                            for (int j = 0; j < name_len; j++) {
                                if (name[j] == ';') break;
                                real_len++;
                            }
                            char *fname = malloc(256);
                            if (!fname)
                            {
                                kprintf("<(0c)>Memory allocation error\n");
                                return;
                            }
                            strncpy(fname, name, real_len);
                            fname[real_len] = 0;
                            kprintf(" %s\n", fname);
                            mfree(fname);
                        }
                        int entry_len = len;
                        if ((33 + name_len) % 2 != 0) entry_len++;
                        offset += len;
                    }
                } 
                else if (strcmp(subarg[0], "cd") == 0) {
                    if (sub_count < 2) {
                        kprint("Usage: cd <directory>\n");
                    } else {
                        toupper(subarg[1]);
                        if (subarg[1][0] == '/' || strncmp(subarg[1], "CDROM:/", 7) == 0) {
                            char* p = subarg[1];
                            if (p[0] == '/') p++;
                            if (strncmp(p, "CDROM:/", 7) == 0) p += 7;
                            strcpy(iso_current_dir, p);
                        } else {
                            if (iso_current_dir[0] == 0) {
                                strcpy(iso_current_dir, subarg[1]);
                            } else {
                                strcat(iso_current_dir, "/");
                                strcat(iso_current_dir, subarg[1]);
                            }
                        }
                    }
                }
                else if (strcmp(subarg[0], "pwd") == 0) {
                    if (iso_current_dir[0] == 0) {
                        kprint("cdrom:/\n");
                    } else {
                        kprintf("cdrom:/%s\n", iso_current_dir);
                    }
                }
                else if (strcmp(subarg[0], "clear") == 0) { kclear(); continue; }
                else if (strcmp(subarg[0], "cat") == 0) {
                    if (sub_count < 2) {
                        kprint("Usage: cat <filename>\n");
                        continue;
                    }
                    char* buf = malloc(4096);
                    if (!buf) {
                        kprint("<(0c)>Error allocating memory<(0f)>\n\n");
                        continue;
                    }
                    kprintf("<(0a)>Reading file from ISO...<(0f)>\n");
                    char *fullpath = malloc(512);
                    if (!fullpath) { kprintf("isotools cat: OOM fp\n"); mfree(buf); continue; }
                    char* ppath;
                    if (strncmp(subarg[1], "CDROM:/", 7) == 0) {
                        ppath = subarg[1];
                    } else if (subarg[1][0] == '/') {
                        ppath = subarg[1] + 1;
                    } else if (iso_current_dir[0]) {
                        strcpy(fullpath, iso_current_dir);
                        strcat(fullpath, "/");
                        strcat(fullpath, subarg[1]);
                        ppath = fullpath;
                    } else {
                        ppath = subarg[1];
                    }
                    toupper(ppath);
                    int sz = iso9660_read(ppath, buf, 4095);
                    if (sz < 0) {
                        kprintf("<(0c)>Error reading file from ISO<(0f)>\n\n");
                        mfree(buf);
                        continue;
                    }
                    buf[sz] = 0;
                    kprintf("%s\n", buf);
                    mfree(buf);
                    mfree(fullpath);
                } else if (strcmp(subarg[0], "cp") == 0) {
                    if (sub_count < 3) {
                        kprint("<(0c)>Usage: cp [-r] <src> <dst><(0f)>\n");
                        continue;
                    }
                    int recursive = 0, src_idx = 1;
                    if (strcmp(subarg[1], "-r") == 0) {
                        recursive = 1;
                        src_idx = 2;
                    }
                    // prepare ISO source path
                    char *isosrc = malloc(512);
                    if (!isosrc) { kprint("isotools cp: OOM isosrc\n"); continue; }
                    strcpy(isosrc, iso_current_dir);
                    char* src_arg = subarg[src_idx];
                    char* psrc;
                    if (strncmp(src_arg, "CDROM:/", 7) == 0) psrc = src_arg + 7;
                    else if (src_arg[0] == '/') psrc = src_arg + 1;
                    else if (iso_current_dir[0]) {
                        strcpy(isosrc, iso_current_dir);
                        strcat(isosrc, "/");
                        strcat(isosrc, src_arg);
                        psrc = isosrc;
                    } else {
                        psrc = src_arg;
                    }
                    if (recursive) {
                        isocpy_dir(psrc, subarg[src_idx+1]);
                    } else {
                        isocpy_file(psrc, subarg[src_idx+1]);
                    }
                    mfree(isosrc);
                } else if (strcmp(subarg[0], "help") == 0) {
                    kprintf("isotools commands:\n  mount [devnum]\n  ls\n  cd <directory>\n  pwd\n  cat <filename>\n  cp [-r] <src> <dst>\n  exit\n");
                } else if (strcmp(subarg[0], "cmp") == 0) {
                    if (sub_count < 3) { kprint("Usage: cmp <src_iso> <dst_file>\n"); continue; }
                    char *isosrc = malloc(512); char* psrc;
                    if (!isosrc) { kprint("isotools cmp: OOM isosrc\n"); continue; }
                    if (strncmp(subarg[1], "CDROM:/", 7)==0) psrc = subarg[1]+7;
                    else if (subarg[1][0]=='/') psrc = subarg[1]+1;
                    else if (iso_current_dir[0]) { strcpy(isosrc, iso_current_dir); strcat(isosrc, "/"); strcat(isosrc, subarg[1]); psrc = isosrc; }
                    else psrc = subarg[1];
                    cmpiso_file(psrc, subarg[2]);
                    mfree(isosrc);
                } else {
                    kprintf("<(0c)>Unknown isotools command: %s<(0f)>\n\n", subarg[0]);
                }
            }
            mfree(cmd);
            return;
        } 
        else if (strcmp(arg[0], "logo") == 0) {
            uint8_t *bmp_data = malloc(219 * 87);
            if (!bmp_data) {
                kprintf("<(0c)>Error allocating memory<(0f)>\n\n");
                return;
            }
            int bmp = iso9660_read("cdrom:/focus/images/logo.bmp", bmp_data, 219 * 87);
            if (bmp <= 0) {
                kprintf("<(0c)>Error reading logo.bmp<(0f)>\n\n");
                mfree(bmp_data);
                return;
            }
            print_bmp16(bmp_data, 219, 87, 800 - 230, 5);
            mfree(bmp_data);
            return;
        }
        else if (strcmp(arg[0], "mkdir") == 0)
        {
            if (count < 2) {
                kprintf("<(0c)>Usage: mkdir [DIRNAME]<(0f)>\n");
                return;
            }
            fat32_dir_entry_t *entries = malloc(32 * sizeof(fat32_dir_entry_t));
            if (!entries) {
                kprintf("<(0c)>Error allocating memory for fat struct entries<(0f)>\n");
                return;
            }
            int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
            if (n < 0 || n >= 32) {
                kprintf("<(0c)>Directory full or error<(0f)>\n");
                mfree(entries);
                return;
            }
            // Directory name (8.3, without dot)
            char name[11];
            memset(name, ' ', 11);
            int i = 0, j = 0;
            while (arg[1][i] && j < 8 && arg[1][i] != '.') name[j++] = my_toupper(arg[1][i++]);
            /* Обрабатываем расширение (если было указано) */
            if (arg[1][i] == '.') {
                i++; int k = 0;
                while (arg[1][i] && k < 3) {
                    name[8 + k] = my_toupper(arg[1][i]);
                    i++; k++;
                }
            }

            /* --- Проверяем, что запись с таким именем ещё не существует --- */
            for (int idx = 0; idx < n; idx++) {
                if (entries[idx].name[0] == 0xE5 || entries[idx].name[0] == 0x00) continue; // удалённая/пустая
                if (memcmp(entries[idx].name, name, 11) == 0) {
                    kprintf("<(0c)>Entry with this name already exists<(0f)>\n");
                    mfree(entries);
                    return;
                }
            }
            // Find free entry
            uint8_t *sector = malloc(512);
            if (!sector)
            {
                kprintf("Error allocating memory\n");
                mfree(entries);
                return -10;
            }
            uint32_t lba = fat32_cluster_to_lba(current_dir_cluster);
            if (ata_read_sector(current_disk, lba, sector) != 0) {
                kprint("<(0c)>Error reading dir<(0f)>\n");
                mfree(sector);
                mfree(entries);
                return;
            }
            for (int off = 0; off < 512; off += 32) {
                if (sector[off] == 0x00 || sector[off] == 0xE5) {
                    // First erase entire sector
                    uint8_t *new_sector = malloc(512);
                    if (!new_sector)
                    {
                        kprintf("Error allocating memory\n");
                        mfree(entries);
                        mfree(sector);
                        return -10;
                    }
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
                    new_sector[off + 11] = 0x10; // mkdir creates directory (attr: 0x10)
                    // --- Выделяем свободный кластер и записываем его ---
                    uint32_t cl = find_free_cluster(current_disk);
                    if (cl == 0) {
                        kprintf("<(0c)>No free cluster<(0f)>\n");
                        mfree(entries);
                        mfree(sector);
                        return;
                    }
                    uint32_t fat_lba = fat_start + (cl * 4) / 512;
                    uint8_t *fat_sec = malloc(512);
                    if (!fat_sec)
                    {
                        kprintf("Error allocating memory\n");
                        mfree(entries);
                        mfree(sector);
                        return -10;
                    }
                    if (ata_read_sector(current_disk, fat_lba, fat_sec) != 0) {
                        kprint("<(0c)>Error reading FAT<(0f)>\n");
                        mfree(entries);
                        mfree(sector);
                        mfree(fat_sec);
                        return;
                    }
                    uint32_t fat_off = (cl * 4) % 512;
                    *(uint32_t*)&fat_sec[fat_off] = 0x0FFFFFFF;
                    if (ata_write_sector(current_disk, fat_lba, fat_sec) != 0) {
                        kprint("<(0c)>Error writing FAT<(0f)>\n");
                        mfree(entries);
                        mfree(sector);
                        mfree(fat_sec);
                        return;
                    }
                    *(uint16_t*)(&new_sector[off + 20]) = (uint16_t)((cl >> 16) & 0xFFFF); // high
                    *(uint16_t*)(&new_sector[off + 26]) = (uint16_t)(cl & 0xFFFF); // low
                    /* заполнить новый каталог записями '.' и '..' */
                    uint8_t dirsec[512]; memset(dirsec, 0, 512);
                    /* '.' */
                    memset(dirsec, ' ', 11); dirsec[0] = '.'; dirsec[11] = 0x10;
                    *(uint16_t*)(&dirsec[20]) = (uint16_t)((cl >> 16) & 0xFFFF);
                    *(uint16_t*)(&dirsec[26]) = (uint16_t)(cl & 0xFFFF);
                    /* '..' */
                    memset(&dirsec[32], ' ', 11); dirsec[32] = '.'; dirsec[33] = '.'; dirsec[43] = 0x10;
                    *(uint16_t*)(&dirsec[52]) = (uint16_t)((current_dir_cluster >> 16) & 0xFFFF);
                    *(uint16_t*)(&dirsec[58]) = (uint16_t)(current_dir_cluster & 0xFFFF);
                    /* записываем сектор */
                    ata_write_sector(current_disk, fat32_cluster_to_lba(cl), dirsec);
                    // Write updated sector
                    if (ata_write_sector(current_disk, lba, new_sector) != 0) {
                        kprint("<(0c)>Error writing dir<(0f)>\n");
                        mfree(entries);
                        mfree(sector);
                        mfree(fat_sec);
                        return;
                    }
                    mfree(fat_sec);
                    mfree(new_sector);
                    mfree(sector);
                    mfree(entries);
                    return;
                }
            }
            kprint("<(0c)>No free entry<(0f)>\n");
            mfree(entries);
            mfree(sector);
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
                kprint("<(0c)>Usage: rm [-r] [PATH]<(0f)>\n");
                return;
            }
            // Determine path
            const char* path = arg[path_idx];
            uint32_t target_cluster = 0;
            fat32_dir_entry_t *entries = malloc(32 * sizeof(fat32_dir_entry_t));
            if (!entries) {
                kprint("<(0c)>Error allocating memory for fat struct entries<(0f)>\n");
                return;
            }
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
                char *parent_path = malloc(256);
                if (!parent_path) {
                    kprint("<(0c)>Error allocating memory for parent path<(0f)>\n");
                    mfree(entries);
                    return;
                }
                strncpy(parent_path, path, last - path);
                parent_path[last - path] = 0;
                uint32_t tmp_cl = 0;
                fat32_resolve_path(current_disk, parent_path, &tmp_cl);
                parent_cluster = tmp_cl;
                mfree(parent_path);
            }
            // Read parent directory
            qemu_debug_printf("parent_cluster: %u\n", parent_cluster);
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
                            uint8_t *sector = malloc(512);
                            if (!sector) { kprint("rm: OOM sec\n"); mfree(entries); return; }
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
                            mfree(sector);
                        } else {
                            kprint("<(0c)>Use rm -r to remove directory<(0f)>\n");
                        }
                    } else {
                        // File
                        uint8_t *sector = malloc(512);
                        if (!sector) {
                            kprint("<(0c)>Error allocating memory for sector<(0f)>\n");
                            mfree(entries);
                            return;
                        }
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
                        mfree(sector);
                    }
                    break;
                }
            }
            if (!found) {
                kprint("<(0c)>Not found<(0f)>\n");
            }
            mfree(entries);
            return;
        }
        else if (strcmp(arg[0], "cd") == 0)
        {
            if (count < 2) {
                kprint("<(0c)>Usage: cd <path><(0f)>\n");
                return;
            }
            int cd_res = fat32_change_dir(current_disk, arg[1]);
            if (cd_res == 0) {
                return;
            } else if (cd_res == -1) {
                kprintf("<(0c)>No such directory: %s<(0f)>\n", arg[1]);
            } else if (cd_res == -2) {
                kprintf("<(0c)>Not a directory: %s<(0f)>\n", arg[1]);
            } else if (cd_res == -3) {
                kprintf("Already at root directory\n");
            } else {
                kprintf("<(0c)>Failed to change directory<(0f)>\n");
            }
            return;
        }
        else if (strcmp(arg[0], "edit") == 0) {
            if (count < 2) {
                kprint("Usage: edit <filename>\n");
                return;
            }
            //editor_main(arg[1], current_disk);
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
                kprintf("Usage: echo [message]\nYou can use <(1e)>color codes<(0f)> to change color of the message.\n");
                return;
            }
            
            // Собираем все аргументы в одну строку
            char *message = malloc(512);
            if (!message) { kprint("echo: OOM\n"); return; }
            memset(message, 0, 512);
            int pos = 0;
            
            for (int i = 1; i < count; i++) {
                // Копируем аргумент
                int j = 0;
                while (arg[i][j] && pos < 511) {
                    message[pos++] = arg[i][j++];
                }
                // Добавляем пробел между аргументами
                if (i < count - 1 && pos < 511) {
                    message[pos++] = ' ';
                }
            }
            message[pos] = '\n';
            message[pos + 1] = 0;
            
            kprintf(message);
            mfree(message);
            return;
        }
        else if (strcmp(arg[0], "sh") == 0)
        {
            if (count < 2)
            {
                char *input = malloc(1024);
                if (!input)
                {
                    kprintf("<(0c)>Error allocating memory<(0f)>\n");
                    return;
                }
                for (;;)
                {
                    print_prompt();
                    get_string(input);
                    shell_execute(input);
                }
                return;
            }
            else{
                shell_execute_fsc(arg[1]);
            }
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
                //fcsasm_compile(arg[1], arg[2]);
                kprint("\n");
            }
        } else if (strcmp(arg[0], "xxd") == 0) {
            /* Usage:
               xxd <file>              – дамп всего файла
               xxd -l <len> <file>     – вывести только len байт            */
            if (count < 2) { kprint("Usage: xxd [-l len] <file>\n"); return; }

            uint32_t len_limit = 0;
            int fidx = 1;
            if (strcmp(arg[1], "-l") == 0) {
                if (count < 4) { kprint("Usage: xxd -l <len> <file>\n"); return; }
                len_limit = atoi(arg[2]);
                if (!len_limit) { kprint("Bad length\n"); return; }
                fidx = 3;
            }
            const char *fname = arg[fidx];

            /* --- ищем файл в текущем каталоге --- */
            fat32_dir_entry_t *dir = malloc(sizeof(fat32_dir_entry_t)*32);
            if (!dir) { kprint("xxd: OOM error\n"); return; }
            int n = fat32_read_dir(current_disk, current_dir_cluster, dir, 32);
            if (n < 0) { kprint("xxd: dir error\n"); return; }

            char fatname[12]; fat_name_from_string(fname, fatname);   /* helper: 8.3 */
            uint32_t clu = 0, fsize = 0;
            for (int i=0;i<n;i++)
                if (!memcmp(dir[i].name,fatname,11)) {
                    clu = (dir[i].first_cluster_high<<16)|dir[i].first_cluster_low;
                    fsize = dir[i].file_size; break;
                }
            if (!clu){ kprintf("xxd: %s not found\n", fname); mfree(dir); return; }

            uint32_t max = (len_limit && len_limit < fsize) ? len_limit : fsize;
            uint8_t buf[16];
            for (uint32_t off=0; off<max; off+=16){
                uint32_t chunk = (max-off>16)?16:max-off;
                fat32_read_file_data(current_disk, fname, buf, chunk, off);

                kprintf("%08X: ", off);
                for(int i=0;i<16;i++){
                    if(i<chunk) kprintf("%02X ", buf[i]);
                    else kprint("   ");
                }
                kprint(" ");
                for(int i=0;i<chunk;i++){
                    char c=(buf[i]>=32&&buf[i]<=126)?buf[i]:'.';
                    kputchar(c,0x07);
                }
                kprint("\n");
            }
            mfree(dir);
            return;
        }
        else if (strcmp(arg[0], "isomount") == 0) {
            int devnum = iso9660_atapi_devnum;
            if (count > 1) devnum = atoi(arg[1]);
            if (iso9660_mount_dev(devnum) == 0) {
                kprintf("ISO9660 successfully mounted (ATAPI #%d)\n", devnum);
            } else {
                kprintf("<(0c)>Error mounting ISO9660<(0f)>\n");
            }
            return;
        } 
        else if (strcmp(arg[0], "testfont") == 0)
        {
            // Testing VBE fonts
            for (int i = 0; i < 256; i++) {
                kprintf("%c", i);
            }
            kprint("\n");
            return;
        }
        else if (strcmp(arg[0], "snake") == 0)
        {
            kprint("WARNING: Snake game may be buggy");
            snake_main();
            return;
        }
        else if (strcmp(arg[0], "sysinfo") == 0)
        {
            sysinfo_command();
            return;
        }
        else if (strcmp(arg[0], "fpkg") == 0)
        {
            if (strcmp(arg[1], "install") == 0)
            {
                
            }
            else if (strcmp(arg[1], "update") == 0)
            {
                
            }
            else if (strcmp(arg[1], "upgrade") == 0)
            {
                
            }
            else if (strcmp(arg[1], "uninstall") == 0)
            {
                
            }
            else
            {
                kprintf("<(0c)>Unknown option: %s\n", arg[1]);
            }
        }
        else if (strcmp(arg[0], "ping") == 0) {
            if (count < 2) { kprint("Usage: ping <a.b.c.d>\n"); return; }
            uint8_t ip_parts[4] = {0};
            char ipcopy[32]; strncpy(ipcopy, arg[1], 31); ipcopy[31]=0;
            char *tok = strtok(ipcopy, "."); int idx=0;
            while (tok && idx<4) { ip_parts[idx++] = atoi(tok); tok = strtok(NULL, "."); }
            if (idx!=4) { kprint("Bad IP format\n"); return; }
            struct net_device *dev = net_get_iface(0);
            if (!dev) { kprint("No network interface\n"); return; }

            /* --- simple ARP resolution --- */
            uint8_t target_mac[6]; int mac_known=0;
            /* if pinging own IP -> use own MAC */
            uint32_t dst_ip = ((uint32_t)ip_parts[0]<<24)|((uint32_t)ip_parts[1]<<16)|((uint32_t)ip_parts[2]<<8)|ip_parts[3];
            if(dst_ip == dev->ip_addr){ memcpy(target_mac, dev->mac,6); mac_known=1; }

            if(!mac_known){
                uint8_t arp_req[14+28]; /* ETH+ARP */
                /* Ethernet */
                for(int i=0;i<6;i++) arp_req[i]=0xff; /* dest broadcast */
                for(int i=0;i<6;i++) arp_req[6+i]=dev->mac[i];
                arp_req[12]=0x08; arp_req[13]=0x06; /* ARP */
                /* ARP payload */
                uint8_t *arp=arp_req+14;
                arp[0]=0x00; arp[1]=0x01; /* HTYPE Ethernet */
                arp[2]=0x08; arp[3]=0x00; /* PTYPE IPv4 */
                arp[4]=6; arp[5]=4;       /* HLEN, PLEN */
                arp[6]=0x00; arp[7]=0x01; /* OPCODE request */
                memcpy(&arp[8], dev->mac,6);                    /* sender MAC */
                arp[14]=(dev->ip_addr>>24)&0xFF; arp[15]=(dev->ip_addr>>16)&0xFF; arp[16]=(dev->ip_addr>>8)&0xFF; arp[17]=dev->ip_addr&0xFF; /* sender IP */
                memset(&arp[18],0,6);                            /* target MAC */
                arp[24]=ip_parts[0]; arp[25]=ip_parts[1]; arp[26]=ip_parts[2]; arp[27]=ip_parts[3]; /* target IP */
                dev->send(dev, arp_req, sizeof(arp_req));

                /* wait for ARP reply up to ~200k iterations */
                for(int t=0;t<200000 && !mac_known;t++){
                    net_poll_once();
                    uint8_t *pkt; uint16_t plen;
                    while(net_dequeue_frame(&pkt,&plen)){
                        if(plen<42) continue;
                        /* Check ARP */
                        if(pkt[12]==0x08 && pkt[13]==0x06){
                            uint8_t *a=pkt+14;
                            if(a[6]==0x00 && a[7]==0x02){ /* reply */
                                /* compare target IP (our IP) and sender IP (dst_ip) */
                                uint32_t sip=(a[14]<<24)|(a[15]<<16)|(a[16]<<8)|a[17];
                                if(sip==dst_ip){ memcpy(target_mac, &a[8],6); mac_known=1; break; }
                            }
                        }
                    }
                }
            }

            if(!mac_known){ kprint("ARP resolution failed\n"); return; }

            kprintf("Pinging to %s...\n", arg[1]);
            #define PING_DATA_SIZE 32
            int sent=0, recv=0;
            for(int seq=1; seq<=4; seq++){
                uint8_t frame[14+20+8+PING_DATA_SIZE];
                /* Ethernet */
                uint8_t *eth=frame;
                for(int i=0;i<6;i++) eth[i]=target_mac[i];
                for(int i=0;i<6;i++) eth[6+i]=dev->mac[i];
                eth[12]=0x08; eth[13]=0x00;
                /* IP */
                uint8_t *ip=frame+14; memset(ip,0,20); ip[0]=0x45;
                uint16_t tot=20+8+PING_DATA_SIZE; ip[2]=tot>>8; ip[3]=tot&0xff;
                ip[8]=64; ip[9]=1;
                ip[12]=(dev->ip_addr>>24)&0xFF; ip[13]=(dev->ip_addr>>16)&0xFF;
                ip[14]=(dev->ip_addr>>8)&0xFF; ip[15]=dev->ip_addr&0xFF;
                ip[16]=ip_parts[0]; ip[17]=ip_parts[1]; ip[18]=ip_parts[2]; ip[19]=ip_parts[3];
                uint32_t s=0; for(int i=0;i<20;i+=2) s+= (ip[i]<<8)|ip[i+1]; while(s>>16) s=(s&0xFFFF)+(s>>16); s=~s; ip[10]=s>>8; ip[11]=s&0xFF;
                /* ICMP */
                uint8_t *icmp=ip+20; icmp[0]=8; icmp[1]=0; icmp[4]=0x12; icmp[5]=0x34; icmp[6]=seq>>8; icmp[7]=seq&0xFF;
                for(int i=0;i<PING_DATA_SIZE;i++) icmp[8+i]=i;
                s=0; for(int i=0;i<8+PING_DATA_SIZE;i+=2) s+= (icmp[i]<<8)|icmp[i+1]; while(s>>16) s=(s&0xFFFF)+(s>>16); s=~s; icmp[2]=s>>8; icmp[3]=s&0xFF;
                dev->send(dev, frame, sizeof(frame)); sent++;

                /* wait up to ~500k iterations */
                int got=0;
                for(int t=0;t<500000 && !got;t++){
                    net_poll_once();
                    uint8_t *pkt; uint16_t plen;
                    while(net_dequeue_frame(&pkt,&plen)){
                        if(plen<42) continue; /* min */
                        if(pkt[12]!=0x08 || pkt[13]!=0x00) continue; /* IPv4 */
                        uint8_t *ip2=pkt+14; if(ip2[9]!=1) continue; /* ICMP */
                        uint8_t *ic2=ip2+ (ip2[0]&0x0F)*4;
                        if(ic2[0]==0 && ic2[1]==0){ /* Echo reply */
                            if(ic2[4]==0x12 && ic2[5]==0x34 && ic2[6]==(seq>>8) && ic2[7]==(seq&0xFF)){
                                uint8_t ttl=ip2[8];
                                kprintf("Answer from %s: byte=%d ttl=%d\n", arg[1], PING_DATA_SIZE, ttl);
                                recv++; got=1; break;
                            }
                        }
                    }
                }
                if(!got) kprintf("Request timeout for seq %d\n", seq);
            }
            kprintf("Success:\n    packet loss: %d%% (sent: %d get: %d)\n", (100*(sent-recv))/sent, sent, recv);
            return;
        }
        else if (strcmp(arg[0], "clear") == 0)
        {
            kclear();
            return;
        }
        else {
            /* --------------------------------------------------------------
             * 1) Handle assignment   BIN=<something>                       
             * -----------------------------------------------------------*/
            if (startsWith(arg[0], "BIN=")) {
                /* copy everything after "BIN=" */
                strncpy(bin_path, arg[0] + 4, sizeof(bin_path) - 1);
                bin_path[sizeof(bin_path) - 1] = 0;
                /* Ensure path ends with slash/backslash for easy concat */
                size_t bl = strlen(bin_path);
                if (bl && bin_path[bl-1] != '/' && bin_path[bl-1] != '\\') {
                    if (bl < sizeof(bin_path) - 1) { bin_path[bl] = '/'; bin_path[bl+1] = 0; }
                }
                kprintf("BIN path set to %s\n", bin_path);
                return;
            }
            else if (startsWith(arg[0], "DRV="))
            {
                strncpy(drv_path, arg[0] + 4, sizeof(drv_path) - 1);
                drv_path[sizeof(drv_path) - 1] = 0;
                /* Ensure path ends with slash/backslash for easy concat */
                size_t bl = strlen(drv_path);
                if (bl && drv_path[bl-1] != '/' && drv_path[bl-1] != '\\') {
                    if (bl < sizeof(drv_path) - 1) { drv_path[bl] = '/'; drv_path[bl+1] = 0; }
                }
                kprintf("DRV path set to %s\n", drv_path);
            }

            /* --------------------------------------------------------------
             * 2) Try to launch the command as .FEX executable               
             * -----------------------------------------------------------*/
            char *fullpath = malloc(512);
            if (!fullpath) { kprintf("shell: OOM fullpath\n"); return; }
            fullpath[0] = 0;

            /* Absolute path supplied? */
            if (startsWith(arg[0], "CDROM:/") || startsWith(arg[0], "cdrom:/") || (arg[0][1] == ':' && (arg[0][2]=='\\' || arg[0][2]=='/'))) {
                strcat(fullpath, arg[0]);
            } else if (bin_path[0]) {
                /* Prepend BIN path */
                strcat(fullpath, bin_path);
                strcat(fullpath, arg[0]);
            } else {
                /* Use as-is (relative to current FAT directory) */
                strcat(fullpath, arg[0]);
            }

            /* Append .FEX if no dot present */
            if (!strchr(fullpath, '.')) {
                strcat(fullpath, ".FEX");
            }

            int launched = 0;
            if (startsWith(fullpath, "CDROM:/") || startsWith(fullpath, "cdrom:/")) {
                launched = run_fex_iso(fullpath);
            } else {
                launched = run_fex_fat(fullpath);
            }

            if (!launched && !startsWith(arg[0], "#")) {
                kprintf("<(0c)>%s: command or executable file not found<(0f)>\n", arg[0]);
            }
            return;
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
    fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
    if (!entries) { kprintf("rm_recursive: OOM\n"); return; }
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
        char *pathbuf = malloc(512);
        if (!pathbuf){ kprint("rm_recursive: OOM pathbuf\n"); mfree(entries); return; }
        strcpy(pathbuf, name);
        // Удаляем запись в текущем каталоге
        fat32_dir_entry_t *del_entries = malloc(sizeof(fat32_dir_entry_t)*32);
        uint8_t *sector = malloc(512);
        if (!del_entries || !sector) { kprintf("rm_recursive: OOM2\n"); if(del_entries) mfree(del_entries); if(sector) mfree(sector); mfree(entries); return; }
        int del_n = fat32_read_dir(drive, cluster, del_entries, 32);
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
        mfree(del_entries);
        mfree(sector);
        mfree(pathbuf);
    }
    mfree(entries);
}

void load_and_run_binary(const char* filename, uint32_t disk, uint32_t dir_cluster) {
    /* Locate directory entry */
    fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
    if (!entries) { kprint("load_and_run_binary: OOM\n"); return; }
    int n = fat32_read_dir(disk, dir_cluster, entries, 32);
    if (n < 0) { kprint("Error reading directory\n"); mfree(entries); return; }

    char fatname[12]; fat_name_from_string(filename, fatname);
    uint32_t cluster=0, size=0;
    for(int i=0;i<n;i++) if(!memcmp(entries[i].name,fatname,11)){
        cluster=(entries[i].first_cluster_high<<16)|entries[i].first_cluster_low;
        size   = entries[i].file_size; break; }
    if(!cluster){ kprintf("File not found: %s\n", filename); mfree(entries); return; }

    /* Read whole file into memory */
    uint8_t *buf = malloc(size);
    if(!buf){ kprint("Out of memory\n"); mfree(entries); return; }
    int rd = fat32_read_file(disk, cluster, buf, size);
    if(rd!= (int)size){ kprint("Error reading file\n"); mfree(buf); mfree(entries); return; }

    /* Load ELF */
    void (*entry)(void);
    if(elf_load_image(buf, size, USER_STACK_TOP, &entry)==0){
        memset((void*)USER_STACK_BASE,0,USER_STACK_SIZE);
        asm volatile (
            "mov  %%esp, %%edi  \n"
            "mov  %0,   %%esp  \n"
            "call *%1          \n"
            "mov  %%edi, %%esp  \n"
            : : "r"(USER_STACK_TOP), "r"(entry) : "edi", "memory" );
        kprintf("user code returned OK (fat direct)\n");
    }
    mfree(buf);
    mfree(entries);
}

// Вспомогательная функция: создать директории по пути (если не существуют)
static int ensure_fat32_path(uint8_t disk, const char* path, uint32_t* out_dir_cluster, char* out_filename) {
    // path: 0:\DIR1\DIR2\file.txt
    // disk: 0
    // out_dir_cluster: кластер каталога, где будет файл
    // out_filename: имя файла (8.3)
    if (!path || !out_dir_cluster || !out_filename) return -1;
    const char* p = path;
    // Пропускаем X:\ или X:/
    if (p[1] == ':' && (p[2] == '\\' || p[2] == '/')) p += 3;
    else if (p[0] == '\\' || p[0] == '/') p++;
    uint32_t cluster = root_dir_first_cluster;
    char part[13];
    int partlen = 0;
    const char* last_sep = p;
    const char* last = p;
    // Найти последнюю компоненту (имя файла)
    for (const char* s = p; *s; s++) {
        if (*s == '\\' || *s == '/') last_sep = s+1;
    }
    // Копируем имя файла
    strncpy(out_filename, last_sep, 12); out_filename[12] = 0;
    // Если нет промежуточных каталогов, сразу возвращаем корневой каталог
    if (last_sep == p) {
        *out_dir_cluster = root_dir_first_cluster;
        return 0;
    }
    // Теперь создаём/ищем все промежуточные каталоги
    const char* s = p;
    while (s < last_sep && *s) {
        // Копируем компоненту
        partlen = 0;
        while (s < last_sep && *s && *s != '\\' && *s != '/') {
            if (partlen < 12) part[partlen++] = *s;
            s++;
        }
        part[partlen] = 0;
        if (partlen > 0) {
            // Проверяем, есть ли такой каталог
            fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t) * 32);
            if (!entries)
            {
                kprintf("Error allocating memory for fat32 entries\n");
                return -1;
            }
            int n = fat32_read_dir(disk, cluster, entries, 32);
            int found = 0;
            uint32_t next_cl = 0;
            for (int i = 0; i < n; i++) {
                if ((entries[i].attr & 0x10) == 0x10) {
                    char name[13] = {0};
                    int pos = 0;
                    for (int j = 0; j < 8; j++) if (entries[i].name[j] != ' ') name[pos++] = entries[i].name[j];
                    if (strcmp(name, part) == 0) {
                        next_cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                        found = 1;
                        mfree(entries);
                        break;
                    }
                }
            }
            mfree(entries);
            if (!found) {
                // Создаём каталог
                // (аналог mkdir)
                uint32_t new_cl = find_free_cluster(disk);
                if (new_cl == 0) return -2;
                // Добавить запись в каталог
                uint8_t *sector = malloc(512);
                if (!sector)
                {
                    kprintf("<(0c)>Error allocating memory for sector\n");
                    return -1;
                }
                uint32_t lba = fat32_cluster_to_lba(cluster);
                if (ata_read_sector(disk, lba, sector) != 0) {mfree(sector); return -1;}
                for (int off = 0; off < 512; off += 32) {
                    if (sector[off] == 0x00 || sector[off] == 0xE5) {
                        memset(&sector[off], ' ', 11);
                        for (int n = 0; n < partlen && n < 8; n++) sector[off + n] = toupper(part[n]);
                        sector[off + 11] = 0x10; // attr: directory
                        *(uint16_t*)(&sector[off + 20]) = (uint16_t)((new_cl >> 16) & 0xFFFF); // high
                        *(uint16_t*)(&sector[off + 26]) = (uint16_t)(new_cl & 0xFFFF); // low
                        if (ata_write_sector(disk, lba, sector) != 0) {mfree(sector); return -1;}
                        break;
                    }
                }
                // Инициализируем новый каталог ('.' и '..')
                uint8_t *newsec = malloc(512);
                if (!newsec)
                {
                    kprintf("<(0c)>Error allocating memory for newsector\n");
                    return -1;
                }
                memset(newsec, 0, 512);
                // .
                memset(&newsec[0], ' ', 11); newsec[0] = '.'; newsec[11] = 0x10;
                *(uint16_t*)(&newsec[20]) = (uint16_t)((new_cl >> 16) & 0xFFFF);
                *(uint16_t*)(&newsec[26]) = (uint16_t)(new_cl & 0xFFFF);
                // ..
                memset(&newsec[32], ' ', 11); newsec[32] = '.'; newsec[33] = '.'; newsec[43] = 0x10;
                *(uint16_t*)(&newsec[52]) = (uint16_t)((cluster >> 16) & 0xFFFF);
                *(uint16_t*)(&newsec[58]) = (uint16_t)(cluster & 0xFFFF);
                if (ata_write_sector(disk, fat32_cluster_to_lba(new_cl), newsec) != 0) {mfree(newsec); return -1;}
                next_cl = new_cl;
            }
            cluster = next_cl;
        }
        if (*s == '\\' || *s == '/') s++;
    }
    *out_dir_cluster = cluster;

    return 0;
}

// Копирование файла из ISO9660 в FAT32 с поддержкой абсолютного пути
static int isocpy_file(const char* src, const char* dst) {
    // 1. Определяем расположение и размер файла в ISO9660
    uint32_t iso_lba, iso_size;
    if (iso9660_find(src, &iso_lba, &iso_size) != 0) {
        kprintf("Read ISO error: %s (not found)\n", src);
        return -1;
    }

    // 2. Определяем целевой каталог и имя файла на FAT32
    uint32_t dir_cluster = current_dir_cluster;
    char filename[13];
    if (dst[1] == ':' && (dst[2] == '\\' || dst[2] == '/')) {
        if (ensure_fat32_path(current_disk, dst, &dir_cluster, filename) != 0) {
            kprintf("Path error: %s\n", dst);
            return -1;
        }
    } else {
        strncpy(filename, dst, 12); filename[12] = 0;
    }

    // 3. Потоковое копирование: читаем с CD сектор за сектором и пишем в FAT32
    const uint32_t SECTOR_SIZE = 2048; // размер ISO-сектора
    uint8_t *buf = malloc(SECTOR_SIZE);
    if (!buf) { kprintf("Alloc error\n"); return -1; }

    uint64_t remaining = iso_size; // может быть > 4 ГБ, но пишем по частям
    uint32_t offset = 0;           // смещение в целевом файле

    // Сохраняем текущий каталог, переходим во временный
    uint32_t saved_cluster = current_dir_cluster;
    current_dir_cluster = dir_cluster;

    for (uint32_t sec = 0; remaining > 0; ++sec) {
        if (atapi_read_device(iso9660_atapi_devnum, iso_lba + sec, 1, buf) != 0) {
            kprintf("Read ISO sector error (LBA=%u)\n", iso_lba + sec);
            current_dir_cluster = saved_cluster;
            mfree(buf);
            return -1;
        }

        // Debug: dump first 16 bytes of ISO sector read, only for first sector
        if (sec == 0) {
            qemu_debug_printf("ISO RD lba %u first16:", iso_lba + sec);
            for (int dbg=0; dbg<16; ++dbg) qemu_debug_printf("%02X ", buf[dbg]);
            qemu_debug_printf("\n");
        }
        uint32_t chunk = (remaining > SECTOR_SIZE) ? SECTOR_SIZE : (uint32_t)remaining;
        int w = fat32_write_file_data(current_disk, filename, buf, chunk, offset);
        if (w != (int)chunk) {
            kprintf("Write error at offset %u (w=%d)\n", offset, w);
            current_dir_cluster = saved_cluster;
            mfree(buf);
            return -1;
        }
        remaining -= chunk;
        offset    += chunk;
    }

    current_dir_cluster = saved_cluster;
    mfree(buf);

    kprintf("<(0a)>Copied %s -> %s (%u bytes)<(0f)>\n", src, dst, iso_size);
    return 0;
}

// Копирование директории из ISO9660 в FAT32 (рекурсивно)
static int isocpy_dir(const char* src, const char* dst) {
    fat32_dir_entry_t *entries = malloc(sizeof(fat32_dir_entry_t)*32);
    if (!entries) { kprintf("isocpy_dir: OOM\n"); return -1; }
    int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
    if (n < 0 || n >= 32) {
        kprintf("isocpy: directory full or error\n");
        mfree(entries);
        return -1;
    }
    char name[12];
    memset(name, ' ', 11); name[11] = 0;
    int clen = strlen(dst);
    for (int i = 0; i < clen && i < 8; i++) name[i] = toupper(dst[i]);
    // Найти свободный кластер
    uint32_t new_cl = find_free_cluster(current_disk);
    if (new_cl == 0) {
        kprintf("isocpy: no free cluster for dir\n");
        mfree(entries);
        return -1;
    }
    // Добавить запись в каталог
    uint8_t *sector = malloc(512);
    if (!sector) { kprintf("isocpy_dir: OOM sec\n"); mfree(entries); return -1; }
    uint32_t lba = fat32_cluster_to_lba(current_dir_cluster);
    if (ata_read_sector(current_disk, lba, sector) != 0) {
        kprintf("isocpy: error reading dir\n");
        mfree(entries); mfree(sector);
        return -1;
    }
    for (int off = 0; off < 512; off += 32) {
        if (sector[off] == 0x00 || sector[off] == 0xE5) {
            memset(&sector[off], ' ', 11);
            for (int n = 0; n < 11; n++) sector[off + n] = name[n];
            sector[off + 11] = 0x10; // attr: directory
            *(uint16_t*)(&sector[off + 20]) = (uint16_t)((new_cl >> 16) & 0xFFFF); // high
            *(uint16_t*)(&sector[off + 26]) = (uint16_t)(new_cl & 0xFFFF); // low
            if (ata_write_sector(current_disk, lba, sector) != 0) {
                kprintf("isocpy: error writing dir\n");
                mfree(entries); mfree(sector);
                return -1;
            }
            break;
        }
    }
    // TODO: рекурсивно копировать содержимое каталога src из ISO9660
    kprintf("isocpy: directory copy not implemented yet\n");
    mfree(entries);
    mfree(sector);
    return -1;
}

// Сравнить файл из ISO9660 с файлом на FAT32, возвращает 0 если идентичны
static int cmpiso_file(const char* src, const char* dst) {
    // Получаем размер ISO-файла
    uint32_t iso_lba, iso_size;
    if (iso9660_find(src, &iso_lba, &iso_size) != 0) {
        kprintf("cmp: ISO file not found: %s\n", src);
        return -1;
    }

    // Определяем расположение файла на FAT32
    uint32_t dir_cluster = current_dir_cluster;
    char filename[13];
    if (dst[1] == ':' && (dst[2] == '\\' || dst[2] == '/')) {
        if (ensure_fat32_path(current_disk, dst, &dir_cluster, filename) != 0) {
            kprintf("cmp: path error: %s\n", dst);
            return -1;
        }
    } else {
        strncpy(filename, dst, 12); filename[12] = 0;
    }

    const uint32_t SECTOR_SIZE = 2048;
    uint8_t *buf_iso = malloc(SECTOR_SIZE);
    uint8_t *buf_fat = malloc(SECTOR_SIZE);
    if (!buf_iso || !buf_fat) { kprintf("cmp: alloc error\n"); return -1; }

    uint64_t remaining = iso_size;
    uint32_t offset = 0;

    uint32_t saved_cluster = current_dir_cluster;
    current_dir_cluster = dir_cluster;

    while (remaining > 0) {
        uint32_t chunk = (remaining > SECTOR_SIZE) ? SECTOR_SIZE : (uint32_t)remaining;
        if (atapi_read_device(iso9660_atapi_devnum, iso_lba + offset/SECTOR_SIZE, 1, buf_iso) != 0) {
            kprintf("cmp: read ISO error\n"); break;
        }
        int r = fat32_read_file_data(current_disk, filename, buf_fat, chunk, offset);
        if (r != (int)chunk) { kprintf("cmp: read FAT error\n"); break; }
        for (uint32_t i=0;i<chunk;i++) {
            if (buf_iso[i]!=buf_fat[i]) {
                kprintf("cmp: mismatch at offset %u (ISO=%02X FAT=%02X)\n", offset+i, buf_iso[i], buf_fat[i]);
                current_dir_cluster = saved_cluster;
                mfree(buf_iso); mfree(buf_fat);
                return 1;
            }
        }
        remaining -= chunk;
        offset += chunk;
    }

    current_dir_cluster = saved_cluster;
    mfree(buf_iso); mfree(buf_fat);
    if (remaining==0) kprintf("cmp: files are identical (%u bytes)\n", iso_size);
    return 0;
}

/* Helpers to run .FEX (ELF) executables */
static int run_fex_fat(const char *path) {
    uint32_t dir_cluster; char fname[13];
    if (ensure_fat32_path(current_disk, path, &dir_cluster, fname)!=0) return 0;

    /* find size */
    fat32_dir_entry_t *dir = malloc(sizeof(fat32_dir_entry_t)*32);
    if (!dir) { kprint("run_fex_fat: OOM\n"); return 1; }
    int n=fat32_read_dir(current_disk, dir_cluster, dir, 32);
    if (n < 0) { kprint("run_fex_fat: dir error\n"); mfree(dir); return 1; }
    uint32_t clu=0, sz=0;
    char fatname[12]; fat_name_from_string(fname,fatname);
    for(int i=0;i<n;i++) if(!memcmp(dir[i].name,fatname,11)){
        clu=(dir[i].first_cluster_high<<16)|dir[i].first_cluster_low;
        sz = dir[i].file_size; break; }
    if(!clu) { mfree(dir); return 0; }

    uint8_t *buf = malloc(sz);
    if(!buf){ kprint("Out of mem\n"); mfree(dir); return 1; }
    int rd = fat32_read_file(current_disk, clu, buf, sz);
    kprintf("FAT read %d / %u bytes (cluster=%u)\n", rd, sz, clu);
    kprintf("BUF[0..31]: ");
    for(int b=0;b<32 && b<rd;b++) kprintf("%02X ", buf[b]);
    kprintf("\n");
    if(rd!=(int)sz){ kprintf("read error\n"); mfree(buf); mfree(dir); return 1; }

    void (*entry)(void);
    if(elf_load_image(buf, sz, USER_STACK_TOP, &entry)==0){
        memset((void*)USER_STACK_BASE,0,USER_STACK_SIZE);
        asm volatile (
            "mov  %%esp, %%edi        \n"
            "mov  %0,   %%esp        \n"
            "call *%1                \n"
            "mov  %%edi, %%esp        \n"
            : /* no outputs */
            : "r"(USER_STACK_TOP), "r"(entry)
            : "edi", "memory" );
        kprintf("user code returned OK (fat)\n");
    }
    mfree(buf);
    mfree(dir);
    return 1;
}

/* --------------------------------------------------------------
 * ISO9660-based .FEX execution helper                            
 * -----------------------------------------------------------*/
static int run_fex_iso(const char *path){
    uint32_t lba, size;
    if(iso9660_find(path, &lba, &size)!=0) return 0;
    uint8_t *buf = malloc(size);
    if(!buf) return 0;
    int rd = iso9660_read(path, buf, size);
    // kprintf("ISO read %d / %u bytes\n", rd, size);
    // kprintf("BUF[0..31]: ");
    // for(int b=0;b<32 && b<rd;b++) kprintf("%02X ", buf[b]);
    // kprintf("\n");
    if(rd!=(int)size){ mfree(buf); return 0; }

    void (*entry)(void);
    if(elf_load_image(buf,size,USER_STACK_TOP,&entry)==0){
        memset((void*)USER_STACK_BASE,0,USER_STACK_SIZE);

        /* Безопасное переключение стека: сохраняем старый ESP в EDI,
           переходим на USER_STACK_TOP, выполняем entry(), возвращаем
           старый ESP.  Пока ESP указывает на пользовательский стек,
           мы НЕ обращаемся к автоматам функции. */
        asm volatile (
            "mov  %%esp, %%edi        \n" /* EDI = old kernel ESP */
            "mov  %0,   %%esp        \n" /* switch to user stack */
            "call *%1                \n" /* execute entry()       */
            "mov  %%edi, %%esp        \n" /* restore kernel ESP    */
            : /* no outputs */
            : "r"(USER_STACK_TOP), "r"(entry)
            : "edi", "memory" );

        kprintf("user code returned OK\n");
    }
    else {
        kprint("Error loading ELF image\n");
    }
    mfree(buf);
    return 1;
}

/* Safe append of zero-terminated src to dest with buffer length limit */
static void str_append(char *dest, const char *src, size_t buf_size)
{
    size_t dlen = strlen(dest);
    size_t i = 0;
    while (src[i] && dlen + i < buf_size - 1) {
        dest[dlen + i] = src[i];
        i++;
    }
    dest[dlen + i] = '\0';
}

/* Convert "name.ext" -> 8.3 (11-byte) FAT name */
static void fat_name_from_string(const char *src, char dest[11])
{
    memset(dest, ' ', 11);
    if (!src) return;
    int dot = -1, len = strlen(src);
    for (int i = 0; i < len; i++)
        if (src[i] == '.') { dot = i; break; }

    if (dot == -1) {
        for (int i = 0; i < len && i < 8; i++)
            dest[i] = my_toupper(src[i]);
    } else {
        for (int i = 0; i < dot && i < 8; i++)
            dest[i] = my_toupper(src[i]);
        for (int i = dot + 1, j = 8; i < len && j < 11; i++, j++)
            dest[j] = my_toupper(src[i]);
    }
}