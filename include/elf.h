#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr_t;

// ELF Magic number
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// ELF Class
#define ELFCLASS32 1

// ELF Data
#define ELFDATA2LSB 1

// ELF Version
#define EV_CURRENT 1

// ELF OS/ABI
#define ELFOSABI_NONE 0

// ELF Type
#define ET_EXEC 2

// ELF Machine
#define EM_386 3

// Program Header Types
#define PT_LOAD 1

// Program Header Flags
#define PF_X 1
#define PF_W 2
#define PF_R 4

#endif // ELF_H 