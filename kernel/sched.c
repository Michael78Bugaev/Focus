#include <sched.h>
#include <mem.h>
#include <vga.h>

static task_t *task_list = NULL;
static task_t *current = NULL;
static uint32_t next_pid = 1;

void scheduler_init(void) {
    /* Create a dummy task – the kernel */
    task_t *idle = malloc(sizeof(task_t));
    if (!idle) return;
    idle->id  = 0;
    idle->esp = 0;
    idle->eip = 0;
    idle->next = idle;
    task_list = current = idle;
}

void scheduler_add(task_t *t) {
    if (!task_list) {
        task_list = current = t;
        t->next = t;
    } else {
        t->next = task_list->next;
        task_list->next = t;
    }
}

/* Minimal implementation: just change the current pointer */
void scheduler_tick(struct InterruptRegisters *regs) {
    (void)regs;
    if (current) current = current->next;
} 