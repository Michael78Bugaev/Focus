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
int fcsasm_parse_and_generate(const char* src, unsigned char* out, int outsize) {
    kprintf("DEBUG: fcsasm_parse_and_generate started!\n");
    int codepos = 0;
    char line[MAX_LINE_LEN];
    int lineno = 0;
    const char* p = src;
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
        // Временно убираю проверку equ
        // if (strstr(s, " equ ") != NULL) continue;
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
            r2 = parse_register(arg2);
            if (r2 == REG_NONE) imm = atoi(arg2);
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
    return codepos;
} 