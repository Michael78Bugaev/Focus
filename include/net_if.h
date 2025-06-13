#ifndef NET_IF_H
#define NET_IF_H

#include <stdint.h>

/* Common representation of a network device visible to upper layers */
struct net_device {
    char name[8];                 // e.g. "eth0"
    uint8_t mac[6];               // MAC address if known
    void   *drv_data;             // driver-private pointer
    const struct net_driver *drv; // driver reference
    uint32_t ip_addr;             // IPv4 address (host order), optional
    /* Transmit one Ethernet frame */
    int (*send)(struct net_device *dev, const uint8_t *buf, uint16_t len);
};

/* Driver descriptor, filled by low-level NIC driver and passed to net_register_driver */
struct net_driver {
    /* Return 1 if this driver can handle given PCI device (bus/dev/fn, vendor, device) */
    int  (*detect)(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t vendor, uint16_t device);
    /* Initialize device, return pointer to net_device or NULL on failure */
    struct net_device *(*init)(uint8_t bus, uint8_t dev, uint8_t fn);
    /* IRQ handler (optional) */
    void (*handle_irq)(struct net_device *dev);
    /* Poll RX, return 1 if frame queued */
    int (*poll)(struct net_device *dev);
};

void net_register_driver(const struct net_driver *drv);
void net_init(void);
void net_probe_pci_device(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t vendor, uint16_t device);
int net_if_count(void);
struct net_device *net_get_iface(int idx);
struct net_device *net_get_iface_by_name(const char *name);
void net_set_ip(struct net_device *dev, uint8_t a, uint8_t b, uint8_t c, uint8_t d);

#endif /* NET_IF_H */ 