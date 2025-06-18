#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <idt.h>

typedef struct task {
    uint32_t esp;
    uint32_t eip;
    struct task *next;
    uint32_t id;
} task_t;

void scheduler_init(void);
void scheduler_add(task_t *t);
//void scheduler_tick(struct InterruptRegisters *);

#endif /* SCHED_H */ 