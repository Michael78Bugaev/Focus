#include <exec.h>
#include <stdint.h>
#include <ports.h>
#include <gdt.h>               /* tss_entry */

/* Глобальные переменные – адрес возврата и стек ядра */
static uint32_t k_ret_eip = 0;
static uint32_t k_ret_esp = 0;

void exec_set_return(uint32_t eip, uint32_t esp)
{
    k_ret_eip = eip;
    k_ret_esp = esp;
}

void exec_return_to_kernel(struct InterruptRegisters *r)
{
    /* Заполняем кадр прерывания так, чтобы iret ушёл в CPL=0 */
    r->csm = 0x08; /* kernel CS */
    r->esp = k_ret_esp;   /* это активный стек CPL=0 */
    r->ss  = 0x10;
    r->ds  = 0x10;
    r->eip = k_ret_eip;
}

void exec_update_tss(uint32_t esp0)
{
    extern struct tss_entry_struct tss_entry;
    tss_entry.esp0 = esp0;
} 