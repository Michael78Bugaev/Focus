#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Read a 32-bit value from PCI configuration space
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

// Enumerate PCI devices and print their IDs and classes
void pci_enumerate(void);

#endif // PCI_H 