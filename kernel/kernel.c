#include <stdint.h>
#include <multiboot2.h>
#include <vbe.h>
#include <vbe_term.h>
#include <terminal.h>

void kernel_main(struct multiboot_tag_framebuffer* fb_info) {
    // Инициализация VBE
    vbe_init(fb_info->framebuffer_addr,
             fb_info->framebuffer_pitch,
             fb_info->framebuffer_width,
             fb_info->framebuffer_height,
             fb_info->framebuffer_bpp);
    
    // Инициализация терминала
    terminal_init();
    
    // Запуск основного цикла терминала
    terminal_loop();
}