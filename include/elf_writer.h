#ifndef ELF_WRITER_H
#define ELF_WRITER_H

#include <elf.h>

void write_elf_header(elf32_ehdr_t* ehdr, uint32_t entry_point, uint32_t phoff);
void write_program_header(elf32_phdr_t* phdr, uint32_t offset, uint32_t vaddr, 
                         uint32_t filesz, uint32_t memsz, uint32_t flags);

#endif // ELF_WRITER_H 