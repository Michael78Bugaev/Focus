#include <vga.h>
#include <fat32.h>
#include <string.h>
#include <fcsasm_parser.h>
#include <fcsasm.h>

// Основная функция компиляции
int fcsasm_compile(const char* src, const char* dst) {
    uint8_t buffer[MAX_CODE_SIZE];
    char filebuf[MAX_CODE_SIZE];

    // --- Преобразуем имя файла в 8.3 формат FAT ---
    char fatname[12];
    memset(fatname, ' ', 11);
    fatname[11] = 0;
    int clen = strlen(src);
    int dot = -1;
    for (int i = 0; i < clen; i++) if (src[i] == '.') { dot = i; break; }
    if (dot == -1) {
        for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(src[i]);
    } else {
        for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(src[i]);
        for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(src[i]);
    }

    // --- Ищем файл в текущей директории ---
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(0, current_dir_cluster, entries, 32);
    uint32_t file_cluster = 0;
    int file_size = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i].name, fatname, 11) == 0) {
            file_cluster = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            file_size = entries[i].file_size;
            break;
        }
    }
    if (file_cluster == 0) {
        kprintf("fcsasm: file not found: %s\n", src);
        return -1;
    }

    // --- Читаем содержимое файла ---
    int size = fat32_read_file(0, file_cluster, (uint8_t*)filebuf, MAX_CODE_SIZE-1);
    kprintf("fcsasm: read %d bytes from %s\n", size, src);
    if (size <= 0) {
        kprintf("fcsasm: cannot read %s\n", src);
        return -1;
    }
    filebuf[size] = 0;

    int code_size = fcsasm_parse_and_generate(filebuf, buffer, sizeof(buffer));
    kprintf("fcsasm: generated %d bytes of code\n", code_size);
    if (code_size < 0) {
        kprintf("fcsasm: compile error\n");
        return -2;
    }
    int res = fat32_write_file(0, dst, buffer, code_size);
    kprintf("fcsasm: fat32_write_file returned %d\n", res);
    if (res < 0) {
        kprintf("fcsasm: cannot write %s\n", dst);
        return -3;
    }
    kprintf("fcsasm: compiled %s (%d bytes)\n", dst, code_size);
    return 0;
}

// main — только для теста, можно убрать
#ifdef FCSASM_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 3) {
        kprintf("Usage: fcsasm <src.asm> <dst.ex>\n");
        return 1;
    }
    return fcsasm_compile(argv[1], argv[2]);
}
#endif 