#include <stdint.h>
#include <vga.h>
#include <pci.h>

// Read a 32-bit value from the PCI configuration space
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31)
                                 | ((uint32_t)bus << 16)
                                 | ((uint32_t)device << 11)
                                 | ((uint32_t)function << 8)
                                 | (offset & 0xFC));
    outportl(0xCF8, address);
    return inportl(0xCFC);
}

// Enumerate PCI devices on bus 0 and print their vendor/device IDs and class codes
void pci_enumerate(void) {
    for (uint8_t bus = 0; bus < 1; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint32_t data = pci_config_read(bus, device, function, 0);
                uint16_t vendor = data & 0xFFFF;
                if (vendor == 0xFFFF) continue;
                uint16_t device_id = (data >> 16) & 0xFFFF;
                uint32_t class_data = pci_config_read(bus, device, function, 8);
                uint8_t class = (class_data >> 24) & 0xFF;
                uint8_t subclass = (class_data >> 16) & 0xFF;
                uint8_t prog_if = (class_data >> 8) & 0xFF;
                kprintf("Found PCI [%d:%d:%d] vendor=0x%04x device=0x%04x class=0x%02x subclass=0x%02x prog_if=0x%02x\n",
                        bus, device, function, vendor, device_id, class, subclass, prog_if);
            }
        }
    }
} 