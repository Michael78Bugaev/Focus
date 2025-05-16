#include <elf.h>
#include <stdint.h>
#include <string.h>

void write_elf_header(elf32_ehdr_t* ehdr, uint32_t entry_point, uint32_t phoff) {
    // Магическое число
    ehdr->e_ident[0] = ELFMAG0;
    ehdr->e_ident[1] = ELFMAG1;
    ehdr->e_ident[2] = ELFMAG2;
    ehdr->e_ident[3] = ELFMAG3;
    
    // 32-битный формат
    ehdr->e_ident[4] = ELFCLASS32;
    
    // Little-endian
    ehdr->e_ident[5] = ELFDATA2LSB;
    
    // Версия ELF
    ehdr->e_ident[6] = EV_CURRENT;
    
    // OS/ABI
    ehdr->e_ident[7] = ELFOSABI_NONE;
    
    // Остальные байты идентификатора
    memset(&ehdr->e_ident[8], 0, 8);
    
    // Тип файла - исполняемый
    ehdr->e_type = ET_EXEC;
    
    // Архитектура - x86
    ehdr->e_machine = EM_386;
    
    // Версия ELF
    ehdr->e_version = EV_CURRENT;
    
    // Точка входа
    ehdr->e_entry = entry_point;
    
    // Смещение к заголовкам программ
    ehdr->e_phoff = phoff;
    
    // Смещение к заголовкам секций (не используется)
    ehdr->e_shoff = 0;
    
    // Флаги (не используются)
    ehdr->e_flags = 0;
    
    // Размер заголовка ELF
    ehdr->e_ehsize = sizeof(elf32_ehdr_t);
    
    // Размер заголовка программы
    ehdr->e_phentsize = sizeof(elf32_phdr_t);
    
    // Количество заголовков программ
    ehdr->e_phnum = 1;
    
    // Размер заголовка секции (не используется)
    ehdr->e_shentsize = 0;
    
    // Количество заголовков секций (не используется)
    ehdr->e_shnum = 0;
    
    // Индекс строковой таблицы секций (не используется)
    ehdr->e_shstrndx = 0;
}

void write_program_header(elf32_phdr_t* phdr, uint32_t offset, uint32_t vaddr, 
                         uint32_t filesz, uint32_t memsz, uint32_t flags) {
    // Тип сегмента - загружаемый
    phdr->p_type = PT_LOAD;
    
    // Смещение в файле
    phdr->p_offset = offset;
    
    // Виртуальный адрес
    phdr->p_vaddr = vaddr;
    
    // Физический адрес (не используется)
    phdr->p_paddr = vaddr;
    
    // Размер в файле
    phdr->p_filesz = filesz;
    
    // Размер в памяти
    phdr->p_memsz = memsz;
    
    // Флаги
    phdr->p_flags = flags;
    
    // Выравнивание
    phdr->p_align = 0x1000;
} 