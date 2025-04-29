unsigned char   inb(unsigned short port);
void    outb(unsigned short port, unsigned char data);
unsigned char   inw(unsigned short port);
void outw(unsigned short port, unsigned short data);

struct InterruptRegisters{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, csm, eflags, useresp, ss;
};
