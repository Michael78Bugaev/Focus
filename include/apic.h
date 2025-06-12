#ifndef APIC_H
#define APIC_H

#include <stdint.h>

// Default physical base address of the local APIC
#define APIC_DEFAULT_BASE    0xFEE00000UL

// Local APIC register offsets (in bytes)
#define APIC_ID              0x020
#define APIC_VERSION         0x030
#define APIC_EOI             0x0B0
#define APIC_SVR             0x0F0
#define APIC_LVT_TIMER       0x320
#define APIC_TIMER_INIT_CNT  0x380
#define APIC_TIMER_CUR_CNT   0x390
#define APIC_TIMER_DIV_CONF  0x3E0

// Timer divisor encodings
#define APIC_DIVIDE_BY_1     0x0B
#define APIC_DIVIDE_BY_2     0x00
#define APIC_DIVIDE_BY_4     0x01
#define APIC_DIVIDE_BY_8     0x02
#define APIC_DIVIDE_BY_16    0x03
#define APIC_DIVIDE_BY_32    0x08
#define APIC_DIVIDE_BY_64    0x09
#define APIC_DIVIDE_BY_128   0x0A

// Timer modes
#define APIC_TIMER_MODE_ONESHOT     0x00000
#define APIC_TIMER_MODE_PERIODIC    0x20000
#define APIC_TIMER_MODE_TSCDEADLINE 0x40000

// APIC timer interrupt vector (must be >32)
#define APIC_TIMER_VECTOR    0x20

// Initialize the local APIC (enable spurious vector)
void apic_init(void);

// Initialize the APIC timer in one-shot or periodic mode
// initial_count: starting counter value
// mode: APIC_TIMER_MODE_*
// vector: interrupt vector to invoke (must match IDT handler)
void apic_timer_init(uint32_t initial_count, uint32_t mode, uint8_t vector);

// Keep track of timer ticks
uint32_t get_apic_ticks(void);

// Reset tick counter to zero
void apic_reset_ticks(void);

#endif // APIC_H 