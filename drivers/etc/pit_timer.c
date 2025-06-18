#include <stdint.h>
#include <ports.h>
#include <idt.h>
#include <vga.h>
#include <pit.h>

uint32_t ticks;
const uint32_t freq = 1000;

int target = 0;
int old_ticks;
int cursor_blink = 0;
// Make it available for other files
extern int cursor_blink;

void on_irq0(struct InterruptRegisters *regs){
    ticks += 1;

    // Blink cursor every 500 ms (freq=1000, so 500 ticks)
    if (ticks % 200 == 0) {
        cursor_blink = !cursor_blink;
        draw_cursor(cursor_blink);
    }
    if (ticks % 9 == 0) {
        vbe_swap();
    }
}

void init_pit(){
    ticks = 0;
    //119318.16666 Mhz
    uint32_t divisor = 1193180/freq;
    qemu_debug_printf("divisor: %d\n", divisor);

    //0011 0110
    qemu_debug_printf("outb(0x43,0x36)\n");
    outb(0x43,0x36);
    qemu_debug_printf("outb(0x40,(uint8_t)(divisor & 0xFF))\n");
    outb(0x40,(uint8_t)(divisor & 0xFF));
    qemu_debug_printf("outb(0x40,(uint8_t)((divisor >> 8) & 0xFF))\n");
    outb(0x40,(uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0,&on_irq0);
}

void pit_sleep(int ms)
{
    old_ticks = ticks;
    target = ms + ticks;

    while (ticks != target)
    {
        __asm__ __volatile__("sti\n\t"  // Enable interrupts
                            "hlt\n\t");  // Wait for interrupt
    }
}