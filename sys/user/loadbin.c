#include <stdint.h>
#include <string.h>
#include <fat32.h>
#include <vga.h>

#define LOAD_ADDR 0x1000
#define STACK_ADDR 0x80000
#define MAX_BIN_SIZE (10ULL * 1024 * 1024 * 1024) // 10 ГБ

// Загружает бинарный файл с диска в память по адресу 0x1000 и передаёт управление
void load_and_run_binary(const char* filename, int current_disk, uint32_t current_dir_cluster) {
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(current_disk, current_dir_cluster, entries, 32);
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
    uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i].name, fatname, 11) == 0 && (entries[i].attr & 0x10) == 0) {
            file_cluster = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            file_size = entries[i].file_size;
            break;
        }
    }
    if (file_cluster == 0) {
        kprintf("loadbin: file not found: %s\n", filename);
        return;
    }
    if (file_size > MAX_BIN_SIZE) {
        kprintf("loadbin: file too large (max 10GB)\n");
        return;
    }
    kprintf("[loadbin] filename: %s\n", filename);
    kprintf("[loadbin] file_cluster: 0x%X, file_size: %u\n", file_cluster, file_size);
    kprintf("[loadbin] LOAD_ADDR: 0x%X, STACK_ADDR: 0x%X\n", LOAD_ADDR, STACK_ADDR);
    uint32_t esp_before, esp_after;
    asm volatile ("movl %%esp, %0" : "=r"(esp_before));
    kprintf("[loadbin] esp before: 0x%X\n", esp_before);
    // Установить стек
    asm volatile ("movl %0, %%esp" : : "r"(STACK_ADDR));
    asm volatile ("movl %%esp, %0" : "=r"(esp_after));
    kprintf("[loadbin] esp after: 0x%X\n", esp_after);
    kprintf("[loadbin] reading file...\n");
    uint8_t* mem = (uint8_t*)LOAD_ADDR;
    int read = fat32_read_file(current_disk, file_cluster, mem, file_size);
    kprintf("[loadbin] read: %d bytes\n", read);
    if (read != file_size) {
        kprintf("loadbin: read error\n");
        return;
    }
    kprintf("[loadbin] transferring control to 0x%X...\n", LOAD_ADDR);
    void (*entry)() = (void (*)())LOAD_ADDR;
    entry();
    // После завершения программа должна вернуть управление (ret)
} 