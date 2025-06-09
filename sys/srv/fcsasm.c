#include <vga.h>
#include <fat32.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Парсинг числа или символа: 'A' -> 65, 0x10 -> 16, 10 -> 10
int parse_imm(const char* s) {
    while (*s == ' ' || *s == ',') s++;
    if (*s == '\'') return (unsigned char)s[1]; // символ в апострофах
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        int val = 0, i = 2;
        while (s[i]) {
            char c = s[i++];
            if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
            else break;
        }
        return val;
    }
    int val = 0, i = 0;
    while (s[i] >= '0' && s[i] <= '9') val = val * 10 + (s[i++] - '0');
    return val;
}

// Функция для приведения строки к нижнему регистру
void to_lower(char* s) {
    while (*s) {
        if (*s >= 'A' && *s <= 'Z') *s += 'a' - 'A';
        s++;
    }
}

// Функция для поиска ключевого слова и аргументов с учётом пробелов
int match_instr(const char* line, const char* instr, char* arg1, char* arg2) {
    // Пропустить пробелы
    while (*line == ' ' || *line == '\t') line++;
    int len = strlen(instr);
    if (strncasecmp(line, instr, len) != 0) return 0;
    line += len;
    // Пропустить пробелы
    while (*line == ' ' || *line == '\t') line++;
    // arg1
    int i = 0;
    while (*line && *line != ' ' && *line != '\t' && *line != ',' && i < 15) arg1[i++] = *line++;
    arg1[i] = 0;
    // Пропустить пробелы и запятые
    while (*line == ' ' || *line == '\t' || *line == ',') line++;
    // arg2
    i = 0;
    while (*line && *line != ' ' && *line != '\t' && *line != ',' && i < 15) arg2[i++] = *line++;
    arg2[i] = 0;
    return 1;
}

// Функция для вставки строки в конец кода и возврата её адреса
int add_string(uint8_t* code, int* code_size, const char* str) {
    int addr = 0x1000 + *code_size;
    int i = 0;
    while (str[i]) {
        code[(*code_size)++] = (uint8_t)str[i++];
    }
    code[(*code_size)++] = 0; // нулевой байт в конец
    return addr;
}

// Мини-компилятор: поддержка только mov reg, imm; int imm; ret; org 0x1000
// Пример исходника:
// org 0x1000
// mov al, 'A'
// mov ah, 0x0E
// int 0x10
// ret

#define MAX_CODE_SIZE 65536

// Для отслеживания последней инструкции mov
char last_mov_reg[8] = {0};
bool last_mov_eax_is_ptr = false;

// Простейший парсер и генератор
int fcsasm_compile(const char* src, const char* dst) {
    char filebuf[MAX_CODE_SIZE];
    uint8_t code[MAX_CODE_SIZE];
    int code_size = 0;
    int org = 0;

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
    //kprintf("fcsasm: read %d bytes from %s\n", size, src);
    if (size <= 0) {
        kprintf("fcsasm: cannot read %s\n", src);
        return -1;
    }
    filebuf[size] = 0;

    // --- Парсим и генерируем код ---
    char* line = strtok(filebuf, "\n");
    while (line) {
        //kprintf("fcsasm: line='%s'\n", line);
        while (*line == ' ' || *line == '\t') line++;
        to_lower(line);
        char arg1[16] = {0}, arg2[16] = {0};
        // equ для строк
        if (match_instr(line, "equ", arg1, arg2)) {
            // arg1 — имя, arg2 — строка
            // Найти кавычки
            char* s = strchr(line, '"');
            if (s) {
                s++;
                char* e = strchr(s, '"');
                if (e) *e = 0;
                int addr = add_string(code, &code_size, s);
                // Можно добавить поддержку меток, если потребуется
            }
            line = strtok(NULL, "\n");
            continue;
        }
        if (match_instr(line, "org", arg1, arg2)) {
            org = parse_imm(arg1);
            line = strtok(NULL, "\n");
            continue;
        }
        if (match_instr(line, "mov", arg1, arg2)) {
            strcpy(last_mov_reg, arg1);
            last_mov_eax_is_ptr = false;
            if (strcmp(arg1, "ah") == 0 && strcmp(arg2, "getcurs") == 0) {
                // mov ah, byte [0x7C00] (оставим для совместимости)
                code[code_size++] = 0x8A; // mov r8, m8
                code[code_size++] = 0x26; // modrm: ah, [disp32]
                code[code_size++] = 0x00;
                code[code_size++] = 0x7C;
                code[code_size++] = 0x00;
                code[code_size++] = 0x00;
            } else if (strcmp(arg1, "al") == 0) {
                int val = parse_imm(arg2);
                code[code_size++] = 0xB0; // mov al, imm8
                code[code_size++] = (uint8_t)val;
            } else if (strcmp(arg1, "ah") == 0) {
                int val = parse_imm(arg2);
                code[code_size++] = 0xB4; // mov ah, imm8
                code[code_size++] = (uint8_t)val;
            } else if (strcmp(arg1, "eax") == 0) {
                // mov eax, label/ptr/строка
                last_mov_eax_is_ptr = true;
                if (strchr(arg2, '"')) {
                    // mov eax, "строка"
                    int addr = add_string(code, &code_size, strchr(arg2, '"') + 1);
                    code[code_size++] = 0xB8; // mov eax, imm32
                    code[code_size++] = addr & 0xFF;
                    code[code_size++] = (addr >> 8) & 0xFF;
                    code[code_size++] = (addr >> 16) & 0xFF;
                    code[code_size++] = (addr >> 24) & 0xFF;
                } else {
                    int val = parse_imm(arg2);
                    code[code_size++] = 0xB8; // mov eax, imm32
                    code[code_size++] = val & 0xFF;
                    code[code_size++] = (val >> 8) & 0xFF;
                    code[code_size++] = (val >> 16) & 0xFF;
                    code[code_size++] = (val >> 24) & 0xFF;
                }
            }
        } else if (match_instr(line, "int", arg1, arg2)) {
            int val = parse_imm(arg1);
            code[code_size++] = 0xCD; // int imm8
            code[code_size++] = (uint8_t)val;
        } else if (strncmp(line, "ret", 3) == 0) {
            code[code_size++] = 0xC3; // ret
        } else if (match_instr(line, "ldchr", arg1, arg2)) {
            // ldchr: печать символа из ax по позиции bx
            // mov edi, 0xB8000
            code[code_size++] = 0xBF; code[code_size++] = 0x00; code[code_size++] = 0x80; code[code_size++] = 0x0B; code[code_size++] = 0x00;
            // movzx ebx, bx
            code[code_size++] = 0x66; code[code_size++] = 0x0F; code[code_size++] = 0xB7; code[code_size++] = 0xDB;
            // lea edi, [edi + ebx*2]
            code[code_size++] = 0x8D; code[code_size++] = 0x3C; code[code_size++] = 0x5F;
            // mov [edi], ax
            code[code_size++] = 0x66; code[code_size++] = 0x89; code[code_size++] = 0x07;
        } else if (match_instr(line, "ldstr", arg1, arg2)) {
            // ldstr: печать строки из eax по позиции bx
            // mov esi, eax
            code[code_size++] = 0x89; code[code_size++] = 0xC6;
            int loop_start = code_size;
            // mov al, [esi]
            code[code_size++] = 0x8A; code[code_size++] = 0x06;
            // test al, al
            code[code_size++] = 0x84; code[code_size++] = 0xC0;
            // je end
            code[code_size++] = 0x74; int je_end = code_size++; // placeholder
            // mov edi, 0xB8000
            code[code_size++] = 0xBF; code[code_size++] = 0x00; code[code_size++] = 0x80; code[code_size++] = 0x0B; code[code_size++] = 0x00;
            // movzx ebx, bx
            code[code_size++] = 0x66; code[code_size++] = 0x0F; code[code_size++] = 0xB7; code[code_size++] = 0xDB;
            // lea edi, [edi + ebx*2]
            code[code_size++] = 0x8D; code[code_size++] = 0x3C; code[code_size++] = 0x5F;
            // mov [edi], ax
            code[code_size++] = 0x66; code[code_size++] = 0x89; code[code_size++] = 0x07;
            // inc bx
            code[code_size++] = 0x66; code[code_size++] = 0xFF; code[code_size++] = 0xC3;
            // inc esi
            code[code_size++] = 0x46;
            // jmp loop
            int rel_loop = loop_start - (code_size + 2);
            code[code_size++] = 0xEB; code[code_size++] = (unsigned char)rel_loop;
            // end:
            int end_pos = code_size;
            code[je_end] = (unsigned char)(end_pos - (je_end + 1));
        } else if (match_instr(line, "in", arg1, arg2)) {
            code[code_size++] = 0xEC;
        } else if (match_instr(line, "out", arg1, arg2)) {
            code[code_size++] = 0xEE;
        } else if (match_instr(line, "inw", arg1, arg2)) {
            code[code_size++] = 0xED;
        } else if (match_instr(line, "outw", arg1, arg2)) {
            code[code_size++] = 0xEF;
        }
        line = strtok(NULL, "\n");
    }

    // --- Записываем бинарник ---
    int res = fat32_write_file(0, dst, code, code_size);
    //kprintf("fcsasm: fat32_write_file returned %d\n", res);
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