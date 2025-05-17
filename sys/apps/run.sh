i686-elf-gcc -ffreestanding -c testvga.c -o testvga.o
i686-elf-ld -T usrlink.ld -o vga.bin testvga.o --oformat binary
cp vga.bin ../../iso/vga.bin