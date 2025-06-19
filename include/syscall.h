/* include/syscall.h */
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

enum {
    SYS_write = 0,
    SYS_exit  = 1,
    SYS_getch = 2,
};

//void syscall_handler(struct InterruptRegisters *regs);

#endif