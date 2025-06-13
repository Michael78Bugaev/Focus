#include <net_if.h>
#include <pci.h>
#include <mem.h>
#include <stdio.h>
#include <string.h>
extern void kprintf(const char*, ...);
extern void e1000_register_driver(void);

#define MAX_NET_DRV 8
static const struct net_driver *g_drivers[MAX_NET_DRV];
static int g_drv_count = 0;

/* store interfaces */
#define MAX_NET_IFACE 4
static struct net_device *g_ifaces[MAX_NET_IFACE];
static const struct net_driver *g_if_drv[MAX_NET_IFACE];
static int g_if_count = 0;

/* store discovered PCI network endpoints prior to driver init */
#define MAX_NET_DEVS 8
struct pci_net_entry {
    uint8_t bus, dev, fn;
    uint16_t vendor, device;
};
static struct pci_net_entry g_pending[MAX_NET_DEVS];
static int g_pending_cnt = 0;
static void print_ip(uint32_t ip);

#define PKT_QUEUE_SIZE 16
struct pkt {
    uint16_t len;
    uint8_t *data;
};
static struct pkt g_queue[PKT_QUEUE_SIZE];
static int q_head=0,q_tail=0;

static int queue_is_full(void){return ((q_tail+1)%PKT_QUEUE_SIZE)==q_head;}
static int queue_is_empty(void){return q_head==q_tail;}
void net_queue_frame(uint8_t *data,uint16_t len){if(queue_is_full()) return; g_queue[q_tail].data=data;g_queue[q_tail].len=len; q_tail=(q_tail+1)%PKT_QUEUE_SIZE;}

int net_dequeue_frame(uint8_t **data,uint16_t *len){ if(queue_is_empty()) return 0; *data=g_queue[q_head].data; *len=g_queue[q_head].len; q_head=(q_head+1)%PKT_QUEUE_SIZE; return 1; }

void net_poll_once(void){
    for(int i=0;i<g_if_count;i++){
        if(g_if_drv[i] && g_if_drv[i]->poll)
            g_if_drv[i]->poll(g_ifaces[i]);
    }
}

void net_register_driver(const struct net_driver *drv)
{
    if (g_drv_count < MAX_NET_DRV)
        g_drivers[g_drv_count++] = drv;
}

void net_probe_pci_device(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t vendor, uint16_t device)
{
    /* just remember the device; actual driver init happens later in net_init */
    if (g_pending_cnt < MAX_NET_DEVS) {
        g_pending[g_pending_cnt++] = (struct pci_net_entry){ bus, dev, fn, vendor, device };
    }
}

void net_init(void)
{
    /* ensure built-in drivers are registered */
    e1000_register_driver();

    /* iterate over pending PCI NICs, find driver, init */
    for (int i = 0; i < g_pending_cnt; ++i) {
        struct pci_net_entry *e = &g_pending[i];
        for (int d = 0; d < g_drv_count; ++d) {
            if (g_drivers[d]->detect(e->bus, e->dev, e->fn, e->vendor, e->device)) {
                struct net_device *nd = g_drivers[d]->init(e->bus, e->dev, e->fn);
                if (nd && g_if_count < MAX_NET_IFACE) {
                    nd->name[0] = 'e'; nd->name[1] = 't'; nd->name[2] = 'h';
                    nd->name[3] = '0' + g_if_count; nd->name[4] = 0;
                    g_ifaces[g_if_count] = nd;
                    g_if_drv[g_if_count] = g_drivers[d];
                    g_if_count++;
                    net_set_ip(nd, 10, 0, 2, 15); /* default for QEMU user-mode */
                    kprintf("NET: detected %s (vendor=%04x dev=%04x, MAC %02x:%02x:%02x:%02x:%02x:%02x, IP ", nd->name, e->vendor, e->device,
                            nd->mac[0],nd->mac[1],nd->mac[2],nd->mac[3],nd->mac[4],nd->mac[5]);
                    print_ip(nd->ip_addr);
                    kprintf(")\n");
                }
                break; /* driver found */
            }
        }
    }
}

/* Removed constructor: net_init will be called explicitly from kernel */ 

int net_if_count(void) { return g_if_count; }

struct net_device *net_get_iface(int idx)
{
    if (idx < 0 || idx >= g_if_count) return 0;
    return g_ifaces[idx];
}

struct net_device *net_get_iface_by_name(const char *name)
{
    for (int i = 0; i < g_if_count; ++i)
        if (!strcmp(g_ifaces[i]->name, name))
            return g_ifaces[i];
    return 0;
}

void net_set_ip(struct net_device *dev, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    dev->ip_addr = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

static void print_ip(uint32_t ip)
{
    kprintf("%d.%d.%d.%d", (ip>>24)&0xFF, (ip>>16)&0xFF, (ip>>8)&0xFF, ip&0xFF);
} 