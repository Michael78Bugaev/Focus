#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Read a 32-bit value from PCI configuration space
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

// Enumerate PCI devices and print their IDs and classes
void pci_enumerate(void);

// Read a 16-bit value from PCI configuration space
uint16_t pci_read_config16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

// Read a 32-bit value from PCI configuration space
uint32_t pci_read_config32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

// Write a 16-bit value to PCI configuration space
void pci_write_config16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);

#endif // PCI_H 