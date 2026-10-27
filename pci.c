
#include "pci.h"
#include "console.h"

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

static void pci_print_device(
    uint8_t bus,
    uint8_t device,
    uint8_t function)
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

    console_write("PCI ");
    console_hex8(bus);
    console_putc(':');
    console_hex8(device);
    console_putc('.');
    console_hex8(function);

    console_write("  Vendor=");
    console_hex16(vendor);

    console_write(" Device=");
    console_hex16(id);

    console_write(" Class=");
    console_hex8(class_code);
    console_putc(':');
    console_hex8(subclass);

    console_write(" IF=");
    console_hex8(prog_if);

    console_write(" Rev=");
    console_hex8(revision);

    console_putc('\n');

    if (vendor == 0x8086 && id == 0x2415) {
      console_write("found audio device\n");
      console_write("Command/Status=");
      console_hex32(pci_read_config(bus, device, function, 0x04));
      console_write(" BAR0=");
      console_hex32(pci_read_config(bus, device, function, 0x10));
      console_write(" BAR1=");
      console_hex32(pci_read_config(bus, device, function, 0x14));
      console_write(" BAR2=");
      console_hex32(pci_read_config(bus, device, function, 0x18));
      console_write(" BAR3=");
      console_hex32(pci_read_config(bus, device, function, 0x1C));
      console_write(" BAR4=");
      console_hex32(pci_read_config(bus, device, function, 0x20));
      console_write(" Interrupt line/pin=");
      console_hex32(pci_read_config(bus, device, function, 0x3C));
      console_write("\n");

      uint16_t nabm = 0xC400;

      console_write("AC97 Global Control: ");
      console_hex16(inw(nabm + 0x2C));
      console_write("\n");

      console_write("AC97 Global Status: ");
      console_hex16(inw(nabm + 0x30));
      console_write("\n");

      console_write("AC97 PCM Out Status: ");
      console_hex8(inb(nabm + 0x16));
      console_write("\n");

      uint32_t bar0 = pci_read_config(bus, device, function, 0x10);
      uint32_t bar1 = pci_read_config(bus, device, function, 0x14);

      uint16_t mixer_base = (uint16_t)(bar0 & ~0x3u);
      uint16_t busmaster_base = (uint16_t)(bar1 & ~0x3u);

      console_write("AC97 mixer base: ");
      console_hex32(mixer_base);
      console_write("\nAC97 busmaster base: ");
      console_hex32(busmaster_base);
      console_write("\n");

      console_write("Busmaster global control: ");
      console_hex32(inl(busmaster_base + 0x2C));
      console_write("\nBusmaster global status: ");
      console_hex32(inl(busmaster_base + 0x30));
      console_write("\n");

      uint16_t pcm_out = busmaster_base + 0x10;

      console_write("PCM Out BDBAR: ");
      console_hex32(inl(pcm_out + 0x00));
      console_write("\n");

      console_write("PCM Out CIV: ");
      console_hex32(inb(pcm_out + 0x04));
      console_write("\n");

      console_write("PCM Out LVI: ");
      console_hex32(inb(pcm_out + 0x05));
      console_write("\n");

      console_write("PCM Out Status: ");
      console_hex32(inw(pcm_out + 0x06));
      console_write("\n");

      console_write("PCM Out PICB: ");
      console_hex32(inw(pcm_out + 0x08));
      console_write("\n");

      console_write("PCM Out Control: ");
      console_hex32(inb(pcm_out + 0x0B));
      console_write("\n");

      //      mixer_base = 0xC000;

      console_write("AC97 Mixer Reset: ");
      console_hex16(inw(mixer_base + 0x00));
      console_write("\n");

      console_write("AC97 Master Volume: ");
      console_hex16(inw(mixer_base + 0x02));
      console_write("\n");

      console_write("AC97 Extended Audio ID: ");
      console_hex16(inw(mixer_base + 0x28));
      console_write("\n");

      console_write("AC97 Powerdown Control: ");
      console_hex16(inw(mixer_base + 0x26));
      console_write("\n");

      outw(mixer_base + 0x02, 0x1010);

      console_write("AC97 Master Volume After Write: ");
      console_hex16(inw(mixer_base + 0x02));
      console_write("\n");      
    }
}

void pci_scan(void)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint16_t vendor = pci_read16(
                (uint8_t)bus, device, 0, 0x00);

            if (vendor == 0xFFFF)
                continue;

            pci_print_device(
                (uint8_t)bus, device, 0);

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
                        function);
                }
            }
        }
    }
}
