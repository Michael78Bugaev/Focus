#include <stdint.h>
#include <vga.h>
#include <multiboot.h>
#include <ata.h>
#include <atapi.h>
#include <ports.h>
//#include <cmos.h>

extern struct multiboot_info* mbi;

// Function to detect if running in QEMU
static int detect_qemu() {
    // Try to read QEMU-specific port
    outb(0x402, 0x10);
    if (inb(0x402) == 0x10) {
        return 1;
    }
    return 0;
}

// Function to detect if running in VMware
static int detect_vmware() {
    // Try VMware I/O port
    outb(0x5658, 0x0A);
    if (inb(0x5658) == 0x0A) {
        return 1;
    }
    return 0;
}

// Function to get CPU vendor string
static void get_cpu_vendor(char* vendor) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                 : "a" (0));
    
    // Copy vendor string
    *(uint32_t*)vendor = ebx;
    *(uint32_t*)(vendor + 4) = edx;
    *(uint32_t*)(vendor + 8) = ecx;
    vendor[12] = '\0';
}

// Function to get total memory in MB
static uint32_t get_total_memory(struct multiboot_info* mbi) {
    if (mbi->flags & 0x01) {
        return (mbi->mem_upper + 1024) / 1024; // Convert to MB
    }
    return 0;
}

void sysinfo_command(struct multiboot_info* mbi) {
    char vendor[13];
    get_cpu_vendor(vendor);
    
    // Get memory info
    uint32_t total_mem = get_total_memory(mbi);
    
    // Get disk info
    ata_drive_t* drive = ata_get_drive(0);
    char disk_model[41] = "Unknown";
    if (drive && drive->present) {
        strncpy(disk_model, drive->name, 40);
        disk_model[40] = '\0';
    }
    
    // Detect virtualization
    int is_qemu = detect_qemu();
    int is_vmware = detect_vmware();
    
    // Print system information
    kprintf("System Information:\n");
    kprintf("------------------\n");
    kprintf("CPU: %s\n", vendor);
    kprintf("RAM: %d MB\n", total_mem);
    kprintf("Disk: %s\n", disk_model);
    kprintf("Using QEMU: %s\n", is_qemu ? "yes" : "no");
    kprintf("Using VMware: %s\n", is_vmware ? "yes" : "no");
    
    // Get date and time from CMOS
    uint8_t second, minute, hour, day, month, year;
    get_date_time(&second, &minute, &hour, &day, &month, &year);
    kprintf("System Time: %02d:%02d:%02d\n", hour, minute, second);
    kprintf("System Date: %02d/%02d/20%02d\n", day, month, year);
} 