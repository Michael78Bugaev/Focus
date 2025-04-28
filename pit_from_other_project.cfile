#include <cpu/pit.h>
#include <vga.h>
#include <io/iotools.h>
#include <io/idt.h>
#include <stdbool.h>
#include <stdio.h>
#include <config.h>
#include <drv/vbe.h>
#include <stdint.h>

uint64_t ticks;
const uint32_t freq = 1000;
int target = 0;
int old_ticks;
bool end = false;

static bool cursor_visible = true; // Состояние видимости курсора
static const unsigned int blink_interval = 500; // Интервал мигания в миллисекундах
static unsigned int last_blink_time = 0; // Время последнего мигания

void inwait(struct InterruptRegisters *regs);

void init_pit()
{
    INFO("Installing IRQ 0 for timer");
    ticks = 0;
    target = 0;
    irq_install_handler(0, &pit_handler);
    uint32_t div = 1193180 / freq;

    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, (uint8_t)div & 0xFF);
    port_byte_out(0x40, (uint8_t)((div >> 8) & 0xFF));

}
void pit_handler(struct InterruptRegisters *regs)
{
    ticks += 1;

    // if (ticks * (1000 / TICKS_PER_SECOND) - last_blink_time >= blink_interval) {
    //     cursor_visible = !cursor_visible; // Меняем состояние видимости курсора
    //     last_blink_time = ticks * (1000 / TICKS_PER_SECOND); // Обновляем время последнего мигания
    
    // // Здесь вы можете добавить код для отрисовки курсора, если он видим
    // if (cursor_visible) 
    // {
    //     for (int x = 0; x < 8; x++)
    //     {
    //         for (int y = 0; y < 8; y++)
    //         {
    //             int xpos = _globl_cursor.x * 8;
    //             int ypos = _globl_cursor.y * 8;
    //             pixel(_GLOBAL_MBOOT, xpos + x, ypos + y, 0x07);
    //         }
    //     }
    // } 
    // else 
    // {
    //     for (int x = 0; x < 8; x++)
    //     {
    //         for (int y = 0; y < 8; y++)
    //         {
    //             int xpos = _globl_cursor.x * 8;
    //             int ypos = _globl_cursor.y * 8;
    //             pixel(_GLOBAL_MBOOT, xpos + x, ypos + y, 0x00);
    //         }
    //     }
    // }
    // }
    
}


void wait(int ms)
{
    old_ticks = ticks;
    target = ms + ticks;

    while (ticks < target)
    {
        
    }
    
}

uint64_t get_ticks()
{
    return ticks;
}

void set_ticks(uint64_t tticks)
{
    ticks = tticks;
}

void debug_handler(struct InterruptRegisters *regs)
{
    // uint32_t cr2;
    // uint32_t ds;
    // uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    // uint32_t int_no, err_code;
    // uint32_t eip, csm, eflags, useresp, ss;
    ticks++;
    int old = get_cursor();
    set_cursor(0);
    printf("<(70)>edi: %x, esi: %x, eax: %x<(07)>\n", regs->edi, regs->esi, regs->eax);
}