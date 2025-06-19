#pragma once
#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>
#include <idt.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Сохранить точку возврата в ядро перед входом в ring 3 */
void exec_set_return(uint32_t eip, uint32_t esp);

/* Подготовить regs так, чтобы iret вернул выполнение в ядро */
//void exec_return_to_kernel(struct InterruptRegisters *regs);

#ifdef __cplusplus
}
#endif

#endif /* EXEC_H */ 