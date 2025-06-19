#include <stdint.h>
#include <exec.h>

#define USER_DS 0x23  /* пользовательский сегмент данных DPL=3 */
#define USER_CS 0x1B  /* пользовательский сегмент кода   DPL=3 */
#define USER_STACK_TOP 0x010FF000u  /* 17 MiB – 4 KiB */

extern void shell_return_from_user(void);

/*
 * Переход в пользовательский режим.
 * entry – адрес пользовательской функции, которая будет выполнена в ring 3.
 * Процедура:
 *   1) загружаем селекторы данных USER_DS во все сегментные регистры;
 *   2) формируем на стеке ядра фрейм, который ожидает iret:
 *        SS, ESP, EFLAGS, CS, EIP;
 *   3) iret — CPU переключается на CPL=3 и начинает исполнение entry.
 */
void enter_user(void (*entry)(void))
{
    uint32_t esp0;
    asm ("mov %%esp,%0":"=r"(esp0));
    /* Задаём адрес возврата до выхода в CPL=3 */
    exec_set_return((uint32_t)&shell_return_from_user, esp0);
    exec_update_tss(esp0);          // стэк ядра для int 0x80

    asm volatile (
        "cli\n"
        "mov  %0, %%ax\n"
        "mov  %%ax, %%ds\n"
        "mov  %%ax, %%es\n"
        "mov  %%ax, %%fs\n"
        "mov  %%ax, %%gs\n"
        /* --- формируем стек, который увидит iret --- */
        "pushl %0\n"            /* SS   */
        "pushl %1\n"            /* ESP  */
        "pushl $0x200\n"        /* EFLAGS с IF=1        */
        "pushl %2\n"            /* CS   */
        "pushl %3\n"            /* EIP  */
        "iret\n"
        : : "i" (USER_DS),
            "r" (USER_STACK_TOP),
            "i" (USER_CS),
            "r" (entry)
        : "ax", "memory");

after_user:
    asm volatile ("sti");
    /* не возвращаемся – на всякий случай */
    shell_return_from_user();
} 