#pragma once
#include <stdint.h>
#include <stddef.h>

struct atapi_device {
    uint8_t channel;
    uint8_t drive;
    uint16_t io_base;
    uint16_t control_base;
    uint8_t slavebit;
    uint8_t exists;
    uint32_t size;
    char model[41];
};

void atapi_init();
struct atapi_device* atapi_get_device(int num);
int atapi_read_device(int devnum, uint32_t lba, uint16_t count, void* buffer);
int atapi_send_packet(struct atapi_device* dev, uint8_t* packet, void* buffer, uint16_t size);
int atapi_identify(struct atapi_device* dev); 