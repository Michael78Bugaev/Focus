#include <net_if.h>
#include <pci.h>
#include <mem.h>
#include <stdint.h>

/* Known device IDs for 82540EM/82545EM etc. (commonly used by VMware/QEMU) */
static const uint16_t e1000_ids[] = { 0x100E, 0x100F, 0x1010, 0x0000 };

#define E1000_REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

/* Register offsets (legacy) */
#define E1000_REG_CTRL   0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_TCTL   0x0400
#define E1000_REG_TIPG   0x0410

#define E1000_REG_TDBAL  0x3800
#define E1000_REG_TDBAH  0x3804
#define E1000_REG_TDLEN  0x3808
#define E1000_REG_TDH    0x3810
#define E1000_REG_TDT    0x3818

/* RX */
#define E1000_REG_RCTL   0x0100
#define E1000_REG_RDBAL  0x2800
#define E1000_REG_RDBAH  0x2804
#define E1000_REG_RDLEN  0x2808
#define E1000_REG_RDH    0x2810
#define E1000_REG_RDT    0x2818
#define E1000_REG_RDTR   0x2820   /* Receive Delay Timer */

#define TX_RING_SIZE 8
#define RX_RING_SIZE 8

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_priv {
    uint32_t mmio;
    uint8_t tx_tail;
    uint8_t rx_head;
};

/* Place rings and buffers in .bss; linker will keep them in low mem (identity-mapped) */
static struct e1000_tx_desc g_tx_descs[TX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t g_tx_bufs[TX_RING_SIZE][2048] __attribute__((aligned(16)));
static struct e1000_rx_desc g_rx_descs[RX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t g_rx_bufs[RX_RING_SIZE][2048] __attribute__((aligned(16)));

static int e1000_send_frame(struct net_device *dev, const uint8_t *buf, uint16_t len);

static int e1000_detect(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t vendor, uint16_t device)
{
    if (vendor != 0x8086) return 0;
    for (int i = 0; e1000_ids[i]; ++i) if (device == e1000_ids[i]) return 1;
    return 0;
}

static struct net_device *e1000_init(uint8_t bus, uint8_t dev, uint8_t fn)
{
    /* Enable memory and bus mastering */
    uint16_t cmd = pci_read_config16(bus, dev, fn, 0x04);
    if(!(cmd & 0x0006)){
        pci_write_config16(bus, dev, fn, 0x04, cmd | 0x0006);
    }

    uint32_t bar0 = pci_read_config32(bus, dev, fn, 0x10);
    if (!(bar0 & 0x1)) { // Memory-mapped
        uint32_t mmio_base = bar0 & ~0xF;
        kprintf("e1000: MMIO at %08x\n", mmio_base);
        struct net_device *nd = (struct net_device*)malloc(sizeof(struct net_device));
        if (!nd) { kprintf("e1000: no mem\n"); return 0; }
        memset(nd, 0, sizeof(*nd));

        /* allocate private structure and TX setup */
        struct e1000_priv *priv = (struct e1000_priv*)malloc(sizeof(struct e1000_priv));
        if (!priv) { kprintf("e1000: no priv mem\n"); mfree(nd); return 0; }
        memset(priv, 0, sizeof(*priv));
        priv->mmio = mmio_base;

        /* init static rings */
        memset(g_tx_descs, 0, sizeof(g_tx_descs));
        memset(g_rx_descs, 0, sizeof(g_rx_descs));
        for(int i=0;i<TX_RING_SIZE;i++){
            g_tx_descs[i].addr=(uint32_t)g_tx_bufs[i];
            g_tx_descs[i].status=0x1;
        }
        for(int i=0;i<RX_RING_SIZE;i++){
            g_rx_descs[i].addr=(uint32_t)g_rx_bufs[i];
        }

        priv->tx_tail = 0;
        priv->rx_head = 0;

        /* program hardware */
        E1000_REG32(mmio_base, E1000_REG_TDBAL) = (uint32_t)g_tx_descs;
        E1000_REG32(mmio_base, E1000_REG_TDBAH) = 0;
        E1000_REG32(mmio_base, E1000_REG_TDLEN) = TX_RING_SIZE * sizeof(struct e1000_tx_desc);
        E1000_REG32(mmio_base, E1000_REG_TDH) = 0;
        E1000_REG32(mmio_base, E1000_REG_TDT) = 0;

        uint32_t tctl = E1000_REG32(mmio_base, E1000_REG_TCTL);
        tctl |= (1 << 1) | (1 << 3); /* EN | PSP */
        E1000_REG32(mmio_base, E1000_REG_TCTL) = tctl;

        /* RX ring setup */
        E1000_REG32(mmio_base, E1000_REG_RDBAL) = (uint32_t)g_rx_descs;
        E1000_REG32(mmio_base, E1000_REG_RDBAH) = 0;
        E1000_REG32(mmio_base, E1000_REG_RDLEN) = RX_RING_SIZE * sizeof(struct e1000_rx_desc);
        E1000_REG32(mmio_base, E1000_REG_RDH) = 0;
        E1000_REG32(mmio_base, E1000_REG_RDT) = RX_RING_SIZE - 1;

        /* минимальное значение RDTR, чтобы QEMU выгружал кадры без IRQ */
        E1000_REG32(mmio_base, E1000_REG_RDTR) = 0x20;

        uint32_t rctl = E1000_REG32(mmio_base, E1000_REG_RCTL);
        /* 2048-байтные буферы: BSIZE = 00b, BSEX = 0 */
        rctl &= ~((3 << 16) | (1 << 25));
        rctl |= (1 << 1);   /* EN */
        rctl |= (1 << 15);  /* BAM – broadcast */
        rctl |= (1 << 26);  /* SECRC – strip CRC */
        E1000_REG32(mmio_base, E1000_REG_RCTL) = rctl;

        nd->drv_data = priv;

        /* Read MAC from RAL0/RAH0 (offsets 0x5400/0x5404 per Intel spec) */
        volatile uint32_t *ral_reg = (volatile uint32_t*)(mmio_base + 0x5400);
        volatile uint32_t *rah_reg = (volatile uint32_t*)(mmio_base + 0x5404);
        uint32_t ral = *ral_reg;
        uint32_t rah = *rah_reg;

        /* Ensure the Address Valid bit is set so the NIC accepts frames for its MAC */
        *ral_reg = ral; /* lower 32 bits unchanged */
        *rah_reg = (rah & 0xFFFF) | (1u<<31); /* upper 16 bits hold MAC[4:5], set AV=1 */

        nd->mac[0] = ral & 0xFF;
        nd->mac[1] = (ral >> 8) & 0xFF;
        nd->mac[2] = (ral >> 16) & 0xFF;
        nd->mac[3] = (ral >> 24) & 0xFF;
        nd->mac[4] = rah & 0xFF;
        nd->mac[5] = (rah >> 8) & 0xFF;

        kprintf("e1000: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                nd->mac[0], nd->mac[1], nd->mac[2], nd->mac[3], nd->mac[4], nd->mac[5]);

        nd->send = e1000_send_frame;
        return nd;
    }
    return 0;
}

static void e1000_irq(struct net_device *dev) { /* stub */ }

static int e1000_send_frame(struct net_device *dev, const uint8_t *buf, uint16_t len)
{
    struct e1000_priv *p = (struct e1000_priv*)dev->drv_data;
    if (len > 1518) return -1;
    uint8_t tail = p->tx_tail;
    struct e1000_tx_desc *desc = &g_tx_descs[tail];
    if (!(desc->status & 0x1)) {
        /* descriptor not done */
        return -2;
    }
    /* copy data */
    memcpy(buf, g_tx_bufs[tail], len);
    desc->addr = (uint32_t)g_tx_bufs[tail];
    desc->length = len;
    desc->cmd = 0x9; /* RS | EOP */
    desc->status = 0;

    /* advance tail */
    tail = (tail + 1) % TX_RING_SIZE;
    p->tx_tail = tail;
    E1000_REG32(p->mmio, E1000_REG_TDT) = tail;
    return 0;
}

static int e1000_poll(struct net_device *dev)
{
    struct e1000_priv *p=(struct e1000_priv*)dev->drv_data;
    int processed=0;
    while(1){
        struct e1000_rx_desc *d=&g_rx_descs[p->rx_head];
        if(!(d->status & 0x1)) break; /* not done */
        uint16_t len=d->length;
        uint8_t *buf=g_rx_bufs[p->rx_head];
        if(processed<5){ /* debug first few frames */
            kprintf("e1000: RX len=%u dst=%02x:%02x:%02x:%02x:%02x:%02x type=%02x%02x\n", len,
                    buf[0],buf[1],buf[2],buf[3],buf[4],buf[5], buf[12],buf[13]);
        }
        /* queue frame to net core */
        net_queue_frame(buf,len);
        d->status=0; /* give back to hw */
        p->rx_head=(p->rx_head+1)%RX_RING_SIZE;
        E1000_REG32(p->mmio,E1000_REG_RDT)= (p->rx_head==0)?(RX_RING_SIZE-1):(p->rx_head-1);
        processed=1;
    }
    return processed;
}

static const struct net_driver e1000_drv = {
    .detect = e1000_detect,
    .init   = e1000_init,
    .handle_irq = e1000_irq,
    .poll = e1000_poll,
};

void e1000_register_driver(void)
{
    net_register_driver(&e1000_drv);
} 