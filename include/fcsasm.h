#ifndef FCSASM_H
#define FCSASM_H

#define MAX_LABELS 128
#define MAX_LABEL_LEN 32
#define MAX_CODE_SIZE 4096
#define MAX_LINE_LEN 256

// Список регистров
typedef enum {
    REG_NONE = 0,
    REG_EAX, REG_EBX, REG_ECX, REG_EDX,
    REG_AH, REG_AL, REG_BH, REG_BL, REG_CH, REG_CL, REG_DH, REG_DL
} Register;

// Список опкодов
typedef enum {
    OP_NOP = 0,
    OP_MOV,
    OP_POP,
    OP_POPA,
    OP_LABEL,
    OP_HLT,
    OP_STRLD, // печать строки
    OP_CHRLD, // печать символа (цвет из AH, символ из AL)
    OP_RET    // возврат
} Opcode; 

// Описание инструкции
typedef struct {
    Opcode opcode;
    Register reg1;
    Register reg2;
    int imm;
    char label[MAX_LABEL_LEN];
} Instruction;

// Метка (лейбл)
typedef struct {
    char name[MAX_LABEL_LEN];
    int addr;
} Label;

// Здесь будут структуры и функции ассемблера

#endif // FCSASM_H 