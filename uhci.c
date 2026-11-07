#include "uhci.h"
#include "serial.h"

#define UHCI_USBCMD     0x00
#define UHCI_USBSTS     0x02
#define UHCI_USBINTR    0x04
#define UHCI_FRNUM      0x06
#define UHCI_FLBASEADD  0x08
#define UHCI_SOFMOD     0x0C
#define UHCI_PORTSC1    0x10
#define UHCI_PORTSC2    0x12

#define UHCI_CMD_RUN       0x0001
#define UHCI_CMD_HCRESET   0x0002
#define UHCI_CMD_GRESET    0x0004
#define UHCI_CMD_CONFIGURE 0x0040
#define UHCI_CMD_MAXP      0x0080

#define UHCI_STS_HALTED    0x0020

static uint16_t base;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1"
                      : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1"
                      : : "a"(value), "Nd"(port));
}

int uhci_init(uint16_t io_base)
{
    base = io_base;

    // uhci_dump_raw_ports("UHCI ports before reset");

    serial_write("UHCI: initialization without reset\n");
    uhci_dump_ports();

    //    outw(base + UHCI_USBCMD, UHCI_CMD_GRESET);

    //    for (volatile unsigned int i = 0; i < 100000; i++)
    //        __asm__ volatile ("pause");

    //    outw(base + UHCI_USBCMD, 0);

    //    serial_write("UHCI: host controller reset\n");

    //    outw(base + UHCI_USBCMD, UHCI_CMD_HCRESET);

    /*    unsigned int timeout = 1000000;

    while ((inw(base + UHCI_USBCMD) & UHCI_CMD_HCRESET) &&
           --timeout) {
        __asm__ volatile ("pause");
    }

    if (timeout == 0) {
        serial_write("UHCI: reset timeout\n");
        return -1;
    }

    uhci_dump_raw_ports("UHCI ports after reset");

    // Disable interrupts for now. */
    outw(base + UHCI_USBINTR, 0);

    serial_write("UHCI: initialization complete\n");

    // Clear pending status bits (write-one-to-clear).
    /*    outw(base + UHCI_USBSTS, 0x003F);

    // Set SOF timing.
    // 0x40 is the normal default.
    outb(base + UHCI_SOFMOD, 0x0040);

    serial_write("UHCI: reset complete\n"); */

    return 0;
}

static void uhci_dump_raw_ports(const char *label)
{
    serial_write(label);
    serial_write("\n");

    serial_write("PORT1: ");
    serial_hex16(inw(base + UHCI_PORTSC1));
    serial_write("\n");

    serial_write("PORT2: ");
    serial_hex16(inw(base + UHCI_PORTSC2));
    serial_write("\n");
}

static void uhci_print_port(const char *name, uint16_t value)
{
    serial_write(name);
    serial_write(" = ");
    serial_hex16(value);

    serial_write(" CCS=");
    serial_hex16(value & 0x0001);

    serial_write(" CSC=");
    serial_hex16(value & 0x0002);

    serial_write(" PE=");
    serial_hex16(value & 0x0004);

    serial_write(" LSDA=");
    serial_hex16(value & 0x0100);

    serial_write("\n");
}

void uhci_dump_ports(void)
{
    uint16_t p1 = inw(base + UHCI_PORTSC1);
    uint16_t p2 = inw(base + UHCI_PORTSC2);

    serial_write("UHCI PORTSC1: ");
    serial_hex16(p1);
    serial_write("\n");

    uhci_print_port("UHCI PORTSC1", p1);

    serial_write("UHCI PORTSC2: ");
    serial_hex16(p2);
    serial_write("\n");

    uhci_print_port("UHCI PORTSC2", p2);
}

