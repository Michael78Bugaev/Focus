#include <kmain.h>
#include <multiboot2.h>
#include <stdint.h>
#include <vbe.h>
#include <vbe_term.h>
#include <gdt.h>
#include <idt.h>
#include <pit.h>

void kmain(uint32_t multiboot_info_addr) {
    uint32_t addr = multiboot_info_addr + 8; // skip total_size and reserved
    struct multiboot_tag *tag;

    uint64_t fb_addr = 0;
    uint32_t fb_pitch = 0, fb_width = 0, fb_height = 0, fb_bpp = 0;

    while (1) {
        tag = (struct multiboot_tag *) addr;
        if (tag->type == 0)
            break;
        if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *) tag;
            fb_addr = fb->framebuffer_addr;
            fb_pitch = fb->framebuffer_pitch;
            fb_width = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_bpp = fb->framebuffer_bpp;
        }
        addr += ((tag->size + 7) & ~7); // align to 8 bytes
    }

    gdt_init();
    idt_init();
    pit_init(100);

    if (fb_addr && fb_bpp == 32) {
        vbe_init((uint32_t)fb_addr, fb_pitch, fb_width, fb_height, fb_bpp);
        vbe_term_init(fb_addr, fb_pitch, fb_width, fb_height, fb_bpp);
        vbe_term_puts("Hello, world!\nTerminal 100x75\n");
        vbe_term_render();
    }

    while (1);
}

// Простейший вывод строки: белые пиксели 8x16, без шрифтов, только ASCII 32-127
static const uint8_t font[16] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // пустой символ
    0x7E,0x81,0xA5,0x81,0xBD,0x99,0x81,0x7E, // 'O' (пример)
    // ... (реальный шрифт нужен для всех символов)
};

static void draw_char(uint32_t fb_addr, uint32_t pitch, uint32_t x, uint32_t y, char c, uint32_t color) {
    (void)c;
    for (uint32_t dy = 0; dy < 16; ++dy) {
        for (uint32_t dx = 0; dx < 8; ++dx) {
            uint32_t *pixel = (uint32_t *)(fb_addr + (y + dy) * pitch + (x + dx) * 4);
            *pixel = color;
        }
    }
}

static void draw_string(uint32_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, const char* str) {
    (void)width;
    (void)height;
    uint32_t x = 10, y = 10;
    while (*str) {
        draw_char(fb_addr, pitch, x, y, *str, 0xFFFFFFFF);
        x += 9;
        ++str;
    }
} 