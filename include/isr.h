#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Interrupt handler function type
typedef void (*isr_t)(void);

// Register an interrupt handler
void register_interrupt_handler(uint8_t n, isr_t handler);

// Initialize interrupt handlers
void isr_init(void);

#endif 