#include <stdint.h>
#include <ports.h>
#include <idt.h>
#include <vga.h>
#include <pit.h>

uint64_t ticks;
const uint32_t freq = 1000;

void on_irq0(struct InterruptRegisters *regs){
    ticks += 1;
    // uint32_t* vga = (uint32_t*)0xb8050;
    // *vga = ticks;
}

void init_pit(){
    ticks = 0;
    irq_install_handler(0,&on_irq0);

    //119318.16666 Mhz
    uint32_t divisor = 1193180/freq;

    //0011 0110
    outb(0x43,0x36);
    outb(0x40,(uint8_t)(divisor & 0xFF));
    outb(0x40,(uint8_t)((divisor >> 8) & 0xFF));
}

void pit_sleep(int sec) {
    // uint64_t start_time = ticks;
    // uint64_t end_time = start_time + (sec * freq);

    // while (ticks < end_time) {
    //     // Ожидаем, пока не пройдет указанное количество секунд
    // }
}