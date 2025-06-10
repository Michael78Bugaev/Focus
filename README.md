<div align="center">

![Focus logo](https://github.com/Michael78Bugaev/Focus/blob/master/images/focus-logo.jpg?v=2)

# Focus

A modern 32-bit operating system written in C and Assembly, featuring a custom shell, FAT32 filesystem support, VBE 800x600 driver with double buffering and a built-in assembly compiler.

![FocusOS Screenshot 1](https://github.com/Michael78Bugaev/Focus/blob/master/images/focus1.png?v=2)


![FocusOS Screenshot 2](https://github.com/Michael78Bugaev/Focus/blob/master/images/focus2.png?v=2)

</div>

## Screenshots

<div align="center">

| Description | Screenshot |
|-------------|------------|
| FocusOS Shell with directory listing and command prompt | ![FocusOS Shell](https://github.com/Michael78Bugaev/Focus/blob/master/images/focus3.png?v=2) |
| Built-in text editor in action | ![FocusOS Editor](https://github.com/Michael78Bugaev/Focus/blob/master/images/focus4.png?v=2) |

</div>

## Features

- **Modern Architecture**
  - 32-bit protected mode
  - Paging and memory management
  - Interrupt handling (IDT)
  - Multiboot compliant

- **File System Support**
  - FAT32 filesystem with full read/write support
  - ISO9660 (CD-ROM) support
  - Directory navigation and file operations
  - Custom file editor

- **Hardware Support**
  - ATA/ATAPI device support
  - VGA text mode (80x25)
  - PS2/USB keyboard support
  - PIT timer

- **Development Tools**
  - FCSASM: Custom assembly compiler (beta)
  - Built-in text editor
  - Hex dump viewer
  - Shell scripting support

## System Requirements

- x86 compatible processor
- 1.5GB RAM (for QEMU)
- ATA/ATAPI compatible storage
- VGA compatible display

## Building and Running

### Prerequisites

- GCC (with 32-bit support)
- NASM
- GNU Make
- GRUB
- QEMU

### Building

```bash
make
```

This will:
1. Compile the kernel and all components
2. Create a bootable ISO image
3. Launch QEMU with the system

### Running in QEMU

The system will automatically start in QEMU after building. To run manually:

```bash
qemu-system-i386 -m 1500M -drive file=hda.img,format=raw,if=ide -cdrom focusos.iso -boot d -serial stdio
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Author

Created by Michael Bugaev

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Project Structure

- `boot/` - Bootloader and early initialization
- `drivers/` - Hardware drivers
- `include/` - Header files
- `kernel/` - Kernel core
- `sys/` - System services and user programs
- `build/` - Build output directory
- `iso/` - ISO image components

## Acknowledgments

- GRUB for bootloader support
- QEMU for system emulation
- The OSDev community for invaluable resources