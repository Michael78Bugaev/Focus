ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
  /* Base address for user-space FEX program */
  . = 0x00400000;

  /* Single LOAD segment – linker places text+rodata+data+bss sequentially */
  .text : {
    *(.text*)
    *(.rodata* .srodata*)
    *(.data*)
    *(.bss* COMMON)
  }
} 