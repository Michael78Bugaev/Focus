#include <vga.h>
#include <fat32.h>
#include <string.h>
#include <fcsasm.h>
#include <fcs_vm.h>

#define VM_MEM_SIZE 1024

void run_executable(const char* filename) {
    uint8_t code[MAX_CODE_SIZE];
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
        kprintf("run: file not found: %s\n", filename);
        return;
    }
    int size = fat32_read_file(0, file_cluster, code, MAX_CODE_SIZE-1);
    if (size <= 0) {
        kprintf("run: cannot read %s\n", filename);
        return;
    }

    // Читаем длину кода из первых 4 байт
    int code_size = code[0] | (code[1] << 8) | (code[2] << 16) | (code[3] << 24);
    int pc = 4;
    int eax = 0, ebx = 0, ecx = 0, edx = 0;
    int ah = 0, al = 0;
    int running = 1;
    int last_pc = 0;
    while (pc < code_size && running) {
        last_pc = pc;
        Opcode op = (Opcode)code[pc++];
        Register r1 = (Register)code[pc++];
        Register r2 = (Register)code[pc++];
        int imm = code[pc++] | (code[pc++] << 8) | (code[pc++] << 16) | (code[pc++] << 24);

        switch (op) {
            case OP_MOV:
                if (r1 == REG_EAX) eax = (r2 != REG_NONE) ? (r2 == REG_EBX ? ebx : (r2 == REG_ECX ? ecx : (r2 == REG_EDX ? edx : imm))) : imm;
                if (r1 == REG_EBX) ebx = (r2 != REG_NONE) ? (r2 == REG_EAX ? eax : (r2 == REG_ECX ? ecx : (r2 == REG_EDX ? edx : imm))) : imm;
                if (r1 == REG_ECX) ecx = (r2 != REG_NONE) ? (r2 == REG_EAX ? eax : (r2 == REG_EBX ? ebx : (r2 == REG_EDX ? edx : imm))) : imm;
                if (r1 == REG_EDX) edx = (r2 != REG_NONE) ? (r2 == REG_EAX ? eax : (r2 == REG_EBX ? ebx : (r2 == REG_ECX ? ecx : imm))) : imm;
                if (r1 == REG_AH) ah = imm;
                if (r1 == REG_AL) al = imm;
                break;
            case OP_POP:
                if (r1 == REG_EAX) eax = 0;
                if (r1 == REG_EBX) ebx = 0;
                if (r1 == REG_ECX) ecx = 0;
                if (r1 == REG_EDX) edx = 0;
                break;
            case OP_POPA:
                eax = ebx = ecx = edx = 0;
                break;
            case OP_STRLD: {
                kprintf("STRLD: eax=%d ah=%d addr=%p str='%s'\n", eax, ah, code + code_size + eax, code + code_size + eax);
                uint8_t color = ah;
                const char* str = (const char*)(code + code_size + eax);
                while (*str) kputchar(*str++, color);
                break;
            }
            case OP_CHRLD: {
                kprintf("CHRLD: al=%d ah=%d\n", al, ah);
                kputchar(al, ah);
                break;
            }
            case OP_RET:
                running = 0;
                 break;
            default:
                kprintf("run: unknown opcode %d\n", op);
                running = 0;
                break;
        }
    }
    //kprintf("run: finished\n");
}