OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS
{
    . = 0x100000;

    .multiboot :
    {
        *(.multiboot)
    }

    .text :
    {
        *(.text.entry)
        *(.text .text.*)
    }

    .data :
    {
        *(.data .data.*)
    }

    .bss :
    {
        *(.bss .bss.*)
    }
}