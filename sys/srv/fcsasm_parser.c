#include <stdio.h>
#include <string.h>
#include <fcsasm_parser.h>
#include <fcsasm.h>

// Вспомогательные функции для парсинга
static Register parse_register(const char* token) {
    if (strcmp(token, "EAX") == 0) return REG_EAX;
    if (strcmp(token, "EBX") == 0) return REG_EBX;
    if (strcmp(token, "ECX") == 0) return REG_ECX;
    if (strcmp(token, "EDX") == 0) return REG_EDX;
    if (strcmp(token, "AH") == 0) return REG_AH;
    if (strcmp(token, "AL") == 0) return REG_AL;
    if (strcmp(token, "BH") == 0) return REG_BH;
    if (strcmp(token, "BL") == 0) return REG_BL;
    if (strcmp(token, "CH") == 0) return REG_CH;
    if (strcmp(token, "CL") == 0) return REG_CL;
    if (strcmp(token, "DH") == 0) return REG_DH;
    if (strcmp(token, "DL") == 0) return REG_DL;
    return REG_NONE;
}

static Opcode parse_opcode(const char* token) {
    if (strcmp(token, "MOV") == 0) return OP_MOV;
    if (strcmp(token, "POP") == 0) return OP_POP;
    if (strcmp(token, "POPA") == 0) return OP_POPA;
    if (strcmp(token, "NOP") == 0) return OP_NOP;
    if (strcmp(token, "HLT") == 0) return OP_HLT;
    if (strcmp(token, "STRLD") == 0) return OP_STRLD;
    if (strcmp(token, "CHRLD") == 0) return OP_CHRLD;
    if (strcmp(token, "RET") == 0) return OP_RET;
    return OP_NOP;
}

void parse_line(const char* line) {
    char buf[MAX_LINE_LEN];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;
    // Удаляем комментарии
    char* comment = strchr(buf, ';');
    if (comment) *comment = 0;
    // Пропускаем пустые строки
    char* p = buf;
    while (isspace(*p)) p++;
    if (*p == 0) return;
    // Проверяем на метку
    char* colon = strchr(p, ':');
    if (colon) {
        *colon = 0;
        kprintf("LABEL: %s\n", p);
        return;
    }
    // Парсим инструкцию
    char* token = strtok(p, " ,\t\n");
    if (!token) return;
    Opcode op = parse_opcode(token);
    if (op == OP_NOP) {
        kprintf("NOP/UNKNOWN: %s\n", token);
        return;
    }
    char* arg1 = strtok(NULL, " ,\t\n");
    char* arg2 = strtok(NULL, " ,\t\n");
    kprintf("INSTR: %d ", op);
    if (arg1) {
        Register r1 = parse_register(arg1);
        if (r1 != REG_NONE) kprintf("R1=%d ", r1);
        else kprintf("IMM1=%s ", arg1);
    }
    if (arg2) {
        Register r2 = parse_register(arg2);
        if (r2 != REG_NONE) kprintf("R2=%d ", r2);
        else kprintf("IMM2=%s ", arg2);
    }
    kprintf("\n");
}

// Главная функция парсера и генератора кода
#define MAX_CONSTS 32
typedef struct {
    char name[MAX_LABEL_LEN];
    int offset;
    char value[MAX_LINE_LEN];
} StringConst;

static StringConst consts[MAX_CONSTS];
static int num_consts = 0;
static int data_offset = 0;
static char data_segment[MAX_CODE_SIZE];

int fcsasm_parse_and_generate(const char* src, unsigned char* out, int outsize) {
    kprintf("DEBUG: fcsasm_parse_and_generate started!\n");
    int codepos = 4; // первые 4 байта — длина кода
    char line[MAX_LINE_LEN];
    int lineno = 0;
    const char* p = src;
    num_consts = 0;
    data_offset = 0;
    while (*p) {
        // Копируем строку
        int l = 0;
        while (*p && *p != '\n' && l < MAX_LINE_LEN-1) line[l++] = *p++;
        line[l] = 0;
        if (*p == '\n') p++;
        lineno++;
        // Удаляем комментарии
        char* comment = strchr(line, ';');
        if (comment) *comment = 0;
        // Пропускаем пустые строки
        char* s = line;
        while (*s && isspace((char)*s)) s++;
        if (*s == 0) continue;
        kprintf("DEBUG: analyzing: '%s'\n", s);
        // Проверка на строковую константу: NAME equ "..."
        char* equ_ptr = strstr(s, " equ ");
        if (equ_ptr) {
            *equ_ptr = 0;
            char* name = s;
            char* val = equ_ptr + 5;
            while (*val && isspace((char)*val)) val++;
            if (*val == '"') {
                val++;
                char* end = strchr(val, '"');
                if (end) *end = 0;
                if (num_consts < MAX_CONSTS) {
                    strncpy(consts[num_consts].name, name, MAX_LABEL_LEN-1);
                    consts[num_consts].name[MAX_LABEL_LEN-1] = 0;
                    strncpy(consts[num_consts].value, val, MAX_LINE_LEN-1);
                    consts[num_consts].value[MAX_LINE_LEN-1] = 0;
                    consts[num_consts].offset = data_offset;
                    int len = strlen(val) + 1;
                    strncpy(data_segment + data_offset, val, len);
                    data_offset += len;
                    num_consts++;
                }
            }
            continue;
        }
        // Метка (LABEL:)
        char* colon = strchr(s, ':');
        if (colon) {
            kprintf("DEBUG: label: '%s'\n", s);
            continue;
        }
        // Парсим инструкцию
        char* token = strtok(s, " ,\t\n");
        if (!token) {
            kprintf("DEBUG: token not found\n");
            continue;
        }
        Opcode op = parse_opcode(token);
        if (op == OP_NOP) {
            kprintf("DEBUG: unknown instruction: '%s'\n", token);
            continue;
        }
        char* arg1 = strtok(NULL, " ,\t\n");
        char* arg2 = strtok(NULL, " ,\t\n");
        Register r1 = REG_NONE, r2 = REG_NONE;
        int imm = 0;
        if (arg1) {
            r1 = parse_register(arg1);
            if (r1 == REG_NONE) imm = atoi(arg1);
        }
        if (arg2) {
            // Проверяем, не является ли arg2 строковой константой
            int found = 0;
            for (int ci = 0; ci < num_consts; ci++) {
                if (strcmp(arg2, consts[ci].name) == 0) {
                    imm = consts[ci].offset;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                r2 = parse_register(arg2);
                if (r2 == REG_NONE) imm = atoi(arg2);
            }
        }
        kprintf("DEBUG: op=%d, r1=%d, r2=%d, imm=%d\n", op, r1, r2, imm);
        // Генерация кода: [OP][R1][R2][IMM]
        if (codepos + 6 > outsize) return -1;
        out[codepos++] = (unsigned char)op;
        out[codepos++] = (unsigned char)r1;
        out[codepos++] = (unsigned char)r2;
        out[codepos++] = (imm >> 0) & 0xFF;
        out[codepos++] = (imm >> 8) & 0xFF;
        out[codepos++] = (imm >> 16) & 0xFF;
        out[codepos++] = (imm >> 24) & 0xFF;
    }
    // Копируем data_segment в конец out[] побайтово
    for (int i = 0; i < data_offset; i++) {
        out[codepos + i] = data_segment[i];
    }
    // Записываем длину кода в первые 4 байта
    int code_size = codepos;
    out[0] = (code_size >> 0) & 0xFF;
    out[1] = (code_size >> 8) & 0xFF;
    out[2] = (code_size >> 16) & 0xFF;
    out[3] = (code_size >> 24) & 0xFF;
    return codepos + data_offset;
}