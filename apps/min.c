#include <stdint.h>
extern void kprintf(const char *fmt, ...);

int main(void)
{
    kprintf("Minimal FEX application: returning to shell.\n");
    return 0;
} 