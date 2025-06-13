#include <stdint.h>
#include <ports.h>
#include <idt.h>
#include <apic.h>
#include <vbe_terminal.h>

// Map the APIC registers at the default physical base
static volatile uint32_t *apic_regs = (volatile uint32_t *)APIC_DEFAULT_BASE;

int cursor_blink = 0;
extern int cursor_blink;

// Write a value to a local APIC register
static inline void apic_write(uint32_t reg, uint32_t value) {
    apic_regs[reg / 4] = value;
    // Read back to ensure the write has posted
    (void)apic_regs[APIC_ID / 4];
}

// Read a value from a local APIC register
static inline uint32_t apic_read(uint32_t reg) {
    return apic_regs[reg / 4];
}

// Spurious-vector enable and basic APIC setup
void apic_init(void) {
    // Disable only PIC timer (IRQ0) but leave keyboard (IRQ1) unmasked
    // Mask slave PIC interrupts (IRQ8-15)
    outb(0xA1, 0xFF);
    // Mask master PIC: mask all IRQs except IRQ1 (keyboard)
    outb(0x21, 0xFD);  // 0xFD = 1111_1101b (unmask bit1)

    // Enable the local APIC by setting the Spurious Interrupt Vector Register (bit 8)
    uint32_t svr = apic_read(APIC_SVR);
    apic_write(APIC_SVR, svr | 0x100);
}

// Keep track of timer ticks
static volatile uint32_t apic_ticks = 0;

void apic_reset_ticks(void) {
    apic_ticks = 0;
}

uint32_t get_apic_ticks(void) {
    return apic_ticks;
}

// Handler for the APIC timer interrupt
void apic_timer_handler(struct InterruptRegisters *regs) {
    // Acknowledge the interrupt
    apic_write(APIC_EOI, 0);
    apic_ticks++;
    // On every 10th tick, swap the VBE framebuffer
    if (apic_ticks % 200 == 0) {
        cursor_blink = !cursor_blink;
        draw_cursor(cursor_blink);
    }
    if (apic_ticks % 9 == 0) {
        vbe_swap();
    }
}

// Initialize the APIC timer
void apic_timer_init(uint32_t initial_count, uint32_t mode, uint8_t vector) {
    // Install our handler at the IRQ index (vector - 32)
    irq_install_handler(vector - 32, apic_timer_handler);

    // Set the divide configuration for maximum resolution (divide by 1)
    apic_write(APIC_TIMER_DIV_CONF, APIC_DIVIDE_BY_1);

    // Configure the LVT Timer register: vector and mode (oneshot/periodic)
    apic_write(APIC_LVT_TIMER, vector | mode);

    // Set the initial count (counter starts decrementing)
    apic_write(APIC_TIMER_INIT_CNT, initial_count);
} 

void apic_timer_sleep(uint32_t ms) {
    uint32_t initial_count = 1193180 / 1000 * ms;
    apic_write(APIC_TIMER_INIT_CNT, initial_count);
    apic_write(APIC_LVT_TIMER, 0x00000000);
}