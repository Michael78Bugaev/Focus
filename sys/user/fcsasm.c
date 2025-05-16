#include <elf.h>
#include "elf_writer.h"

// ... существующий код ...

void fcsasm_compile(const char* src_file, const char* dst_file) {
    // ... существующий код до генерации бинарного файла ...

    // Создаем ELF файл
    elf32_ehdr_t ehdr;
    elf32_phdr_t phdr;
    
    // Вычисляем размеры и смещения
    uint32_t code_size = code_ptr - code;
    uint32_t data_size = data_ptr - data;
    uint32_t bss_size = bss_ptr - bss;
    
    uint32_t phoff = sizeof(elf32_ehdr_t);
    uint32_t code_offset = phoff + sizeof(elf32_phdr_t);
    uint32_t data_offset = code_offset + code_size;
    
    // Записываем заголовок ELF
    write_elf_header(&ehdr, 0x100000, phoff);
    
    // Записываем заголовок программы для кода
    write_program_header(&phdr, code_offset, 0x100000, code_size, code_size, PF_X | PF_R);
    
    // Записываем заголовок программы для данных
    write_program_header(&phdr, data_offset, 0x200000, data_size, data_size + bss_size, PF_W | PF_R);
    
    // Записываем файл
    fat32_write_file(current_disk, current_dir_cluster, dst_file, (uint8_t*)&ehdr, sizeof(elf32_ehdr_t));
    fat32_write_file(current_disk, current_dir_cluster, dst_file, (uint8_t*)&phdr, sizeof(elf32_phdr_t));
    fat32_write_file(current_disk, current_dir_cluster, dst_file, code, code_size);
    fat32_write_file(current_disk, current_dir_cluster, dst_file, data, data_size);
    
    kprintf("Compiled %s to %s\n", src_file, dst_file);
}

// ... существующий код ... 