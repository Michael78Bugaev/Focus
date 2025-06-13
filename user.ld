ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
  /* place user prog at 0x00200000 to avoid kernel space */
  . = 0x00200000;

  .text : { *(.text*) }
  .rodata : { *(.rodata* .srodata*) }
  .data : { *(.data*) }
  .bss  : { *(.bss* COMMON) }
} 