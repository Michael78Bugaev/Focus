// elf.h — заголовочный файл для работы с ELF

#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdbool.h>

#define ELF_NIDENT 16

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

// ELF заголовок
typedef struct {
    uint8_t     e_ident[ELF_NIDENT];
    Elf32_Half  e_type;
    Elf32_Half  e_machine;
    Elf32_Word  e_version;
    Elf32_Addr  e_entry;
    Elf32_Off   e_phoff;
    Elf32_Off   e_shoff;
    Elf32_Word  e_flags;
    Elf32_Half  e_ehsize;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shstrndx;
} Elf32_Ehdr;

// Программный заголовок (Program Header)
typedef struct {
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} Elf32_Phdr;

// Константы для проверки ELF
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5

#define ELFMAG0     0x7F
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'

#define ELFCLASS32  1
#define ELFDATA2LSB 1

#define EM_386      3
#define EV_CURRENT  1

#define PT_LOAD     1

// Проверка ELF-заголовка
static inline bool elf_check_file(const Elf32_Ehdr *hdr) {
    if (!hdr) return false;
    if (hdr->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (hdr->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (hdr->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (hdr->e_ident[EI_MAG3] != ELFMAG3) return false;
    return true;
}

// Проверка поддержки ELF-файла
static inline bool elf_check_supported(const Elf32_Ehdr *hdr) {
    if (!elf_check_file(hdr)) return false;
    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) return false;
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) return false;
    if (hdr->e_machine != EM_386) return false;
    if (hdr->e_version != EV_CURRENT) return false;
    return true;
}

#endif // ELF_H