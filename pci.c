
#include "pci.h"
#include "console.h"
#include "serial.h"

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value)
                      : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      :
                      : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0"
                      : "=a"(value)
                      : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1"
                      :
                      : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile (
        "outl %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;

    __asm__ volatile (
        "inl %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static uint32_t pci_read32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset)
{
    uint32_t address =
        0x80000000u |
        ((uint32_t)bus      << 16) |
        ((uint32_t)device   << 11) |
        ((uint32_t)function << 8)  |
        ((uint32_t)offset & 0xFCu);

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read16(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset)
{
    uint32_t value = pci_read32(
        bus, device, function, offset);

    return (uint16_t)(
        value >> ((offset & 2u) * 8u));
}

static uint32_t pci_read_config(uint8_t bus,
                                uint8_t device,
                                uint8_t function,
                                uint8_t offset)
{
    uint32_t address =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC);

    outl(0xCF8, address);
    return inl(0xCFC);
}

static void uhci_dump_registers(uint16_t base)
{
    serial_write("UHCI I/O base: ");
    serial_hex16(base);
    serial_write("\n");

    serial_write("USBCMD: ");
    serial_hex16(inw(base + 0x00));
    serial_write("\n");

    serial_write("USBSTS: ");
    serial_hex16(inw(base + 0x02));
    serial_write("\n");

    serial_write("USBINTR: ");
    serial_hex16(inw(base + 0x04));
    serial_write("\n");

    serial_write("FRNUM: ");
    serial_hex16(inw(base + 0x06));
    serial_write("\n");

    serial_write("FLBASEADD: ");
    serial_hex32(inl(base + 0x08));
    serial_write("\n");

    serial_write("SOFMOD: ");
    serial_hex8(inb(base + 0x0C));
    serial_write("\n");

    serial_write("PORTSC1: ");
    serial_hex16(inw(base + 0x10));
    serial_write("\n");

    serial_write("PORTSC2: ");
    serial_hex16(inw(base + 0x12));
    serial_write("\n");
}

static void pci_print_device(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint16_t* mixer_base,
    uint16_t* busmaster_base
			     )
{
    uint16_t vendor = pci_read16(
        bus, device, function, 0x00);

    if (vendor == 0xFFFF)
        return;

    uint16_t id = pci_read16(
        bus, device, function, 0x02);

    uint32_t class_info = pci_read32(
        bus, device, function, 0x08);

    uint8_t revision = class_info & 0xFF;
    uint8_t prog_if  = (class_info >> 8) & 0xFF;
    uint8_t subclass = (class_info >> 16) & 0xFF;
    uint8_t class_code = (class_info >> 24) & 0xFF;

    serial_write("PCI ");
    serial_hex8(bus);
    serial_putc(':');
    serial_hex8(device);
    serial_putc('.');
    serial_hex8(function);

    serial_write("  Vendor=");
    serial_hex16(vendor);

    serial_write(" Device=");
    serial_hex16(id);

    serial_write(" Class=");
    serial_hex8(class_code);
    serial_putc(':');
    serial_hex8(subclass);

    serial_write(" IF=");
    serial_hex8(prog_if);

    serial_write(" Rev=");
    serial_hex8(revision);

    serial_putc('\n');

    /* usb controller */
    if (class_code == 0x0C && subclass == 0x03) {
      serial_write("USB controller found: ");

      if (prog_if == 0x00)
        serial_write("UHCI\n");
      else if (prog_if == 0x10)
        serial_write("OHCI\n");
      else if (prog_if == 0x20)
        serial_write("EHCI\n");
      else
        serial_write("Unknown interface\n");

      serial_write("PCI command: ");
      serial_hex16(pci_read16(bus, device, function, 0x04));

      serial_write("\nBAR4: ");
      serial_hex32(pci_read32(bus, device, function, 0x20));

      serial_write("\nInterrupt: ");
      serial_hex32(pci_read32(bus, device, function, 0x3C));

      serial_write("\n");

      uint16_t bar4 = pci_read16(bus, device, function, 0x20);
      uint16_t uhci_base = bar4 & 0xFFF0;

      uhci_dump_registers(uhci_base);      
    }
    /* end usb */

    if (vendor == 0x8086 && (id == 0x2415 || id == 0x2485)) {
      serial_write("found audio device\n");
      serial_write("Command/Status=");
      serial_hex32(pci_read_config(bus, device, function, 0x04));
      serial_write(" BAR0=");
      serial_hex32(pci_read_config(bus, device, function, 0x10));
      serial_write(" BAR1=");
      serial_hex32(pci_read_config(bus, device, function, 0x14));
      serial_write(" BAR2=");
      serial_hex32(pci_read_config(bus, device, function, 0x18));
      serial_write(" BAR3=");
      serial_hex32(pci_read_config(bus, device, function, 0x1C));
      serial_write(" BAR4=");
      serial_hex32(pci_read_config(bus, device, function, 0x20));
      serial_write(" Interrupt line/pin=");
      serial_hex32(pci_read_config(bus, device, function, 0x3C));
      serial_write("\n");

      uint16_t nabm = 0xC400;

      serial_write("AC97 Global Control: ");
      serial_hex16(inw(nabm + 0x2C));
      serial_write("\n");

      serial_write("AC97 Global Status: ");
      serial_hex16(inw(nabm + 0x30));
      serial_write("\n");

      serial_write("AC97 PCM Out Status: ");
      serial_hex8(inb(nabm + 0x16));
      serial_write("\n");

      uint32_t bar0 = pci_read_config(bus, device, function, 0x10);
      uint32_t bar1 = pci_read_config(bus, device, function, 0x14);

      //      uint16_t mixer_base = (uint16_t)(bar0 & ~0x3u);
      //      uint16_t busmaster_base = (uint16_t)(bar1 & ~0x3u);
      *mixer_base = (uint16_t)(bar0 & 0xFFFFFFFC);
      *busmaster_base = (uint16_t)(bar1 & 0xFFFFFFFC);

      serial_write("AC97 mixer base: ");
      serial_hex32(*mixer_base);
      serial_write("\nAC97 busmaster base: ");
      serial_hex32(*busmaster_base);
      serial_write("\n");

      serial_write("Busmaster global control: ");
      serial_hex32(inl(*busmaster_base + 0x2C));
      serial_write("\nBusmaster global status: ");
      serial_hex32(inl(*busmaster_base + 0x30));
      serial_write("\n");

      uint16_t pcm_out = *busmaster_base + 0x10;

      serial_write("PCM Out BDBAR: ");
      serial_hex32(inl(pcm_out + 0x00));
      serial_write("\n");

      serial_write("PCM Out CIV: ");
      serial_hex32(inb(pcm_out + 0x04));
      serial_write("\n");

      serial_write("PCM Out LVI: ");
      serial_hex32(inb(pcm_out + 0x05));
      serial_write("\n");

      serial_write("PCM Out Status: ");
      serial_hex32(inw(pcm_out + 0x06));
      serial_write("\n");

      serial_write("PCM Out PICB: ");
      serial_hex32(inw(pcm_out + 0x08));
      serial_write("\n");

      serial_write("PCM Out Control: ");
      serial_hex32(inb(pcm_out + 0x0B));
      serial_write("\n");

      //      mixer_base = 0xC000;

      serial_write("AC97 Mixer Reset: ");
      serial_hex16(inw(*mixer_base + 0x00));
      serial_write("\n");

      serial_write("AC97 Master Volume: ");
      serial_hex16(inw(*mixer_base + 0x02));
      serial_write("\n");

      serial_write("AC97 Extended Audio ID: ");
      serial_hex16(inw(*mixer_base + 0x28));
      serial_write("\n");

      serial_write("AC97 Powerdown Control: ");
      serial_hex16(inw(*mixer_base + 0x26));
      serial_write("\n");

      outw(*mixer_base + 0x02, 0x1010);

      serial_write("AC97 Master Volume After Write: ");
      serial_hex16(inw(*mixer_base + 0x02));
      serial_write("\n");

      //      uint32_t bar0 = pci_read_config(bus, device, function, 0x10);
      //      uint32_t bar1 = pci_read_config(bus, device, function, 0x14);
    }
}

void pci_scan(uint16_t* mixer_base, uint16_t* busmaster_base)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint16_t vendor = pci_read16(
                (uint8_t)bus, device, 0, 0x00);

            if (vendor == 0xFFFF)
                continue;

            pci_print_device(
			     (uint8_t)bus, device, 0, mixer_base, busmaster_base);

            uint8_t header_type =
                (uint8_t)(pci_read32(
                    (uint8_t)bus, device, 0, 0x0C)
                    >> 16);

            if (header_type & 0x80) {
                for (uint8_t function = 1;
                     function < 8; function++) {
                    pci_print_device(
                        (uint8_t)bus,
                        device,
                        function, mixer_base, busmaster_base);
                }
            }
        }
    }
}
