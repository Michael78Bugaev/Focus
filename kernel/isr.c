#include <stdint.h>
#include "isr.h"
#include "idt.h"
#include "cursor.h"
#include "io.h"
#include "vbe_term.h"

// Array of interrupt handlers
static isr_t interrupt_handlers[256];

// Default interrupt handler
static void default_handler(void) {
    // Acknowledge interrupt
    outb(0x20, 0x20);
}

// Register an interrupt handler
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

// Initialize interrupt handlers
void isr_init(void) {
    // Set all handlers to default
    for (int i = 0; i < 256; i++) {
        interrupt_handlers[i] = default_handler;
    }
}

// PIT interrupt handler
static void pit_handler(void) {
    vbe_term_update_cursor_blink();
    // Acknowledge interrupt
    outb(0x20, 0x20);
}

// Initialize PIT
void pit_init(uint32_t freq) {
    // Calculate divisor
    uint32_t divisor = 1193180 / freq;

    // Send command byte
    outb(0x43, 0x36);

    // Send frequency divisor
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    // Register PIT handler
    register_interrupt_handler(32, pit_handler);
} 