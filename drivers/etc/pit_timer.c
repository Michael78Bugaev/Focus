#include <stdint.h>
#include <ports.h>
#include <idt.h>
#include <vga.h>
#include <pit.h>

uint64_t ticks;
const uint32_t freq = 1000;

int target = 0;
int old_ticks;

void on_irq0(struct InterruptRegisters *regs){
    ticks += 1;
    //vbe_swap();
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
        __asm__ __volatile__("sti\n\t"  // Включаем прерывания
                            "hlt\n\t");  // Ждем прерывания
    }
}