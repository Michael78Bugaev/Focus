#include <syscall.h>
#include <vga.h>
#include <ports.h>
#include <string.h>
#include <idt.h>
#include <exec.h>

void syscall_handler(struct InterruptRegisters *regs)
{
    switch (regs->eax) {
        case SYS_write: {
            const char *s = (const char *)regs->ebx;
            if (s) kprint(s);
            break;
        }
        case SYS_getch: {
            regs->eax = kgetch();
            break;
        }
        case SYS_exit: {
            kprintf("[syscall] SYS_exit from user\n");
            exec_return_to_kernel(regs);
            break;
        }
        default:
            /* пока просто игнор */
            break;
    }
} 