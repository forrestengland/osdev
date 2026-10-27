
#include "serial.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00); // Disable interrupts
    outb(COM1 + 3, 0x80); // Enable divisor latch
    outb(COM1 + 0, 0x01); // Divisor low: 115200 baud
    outb(COM1 + 1, 0x00); // Divisor high
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // Enable and clear FIFOs
    outb(COM1 + 4, 0x0B); // Enable IRQs, RTS, DTR
}

void serial_putc(char c)
{
    // Wait until the transmitter holding register is empty.
    while (!(inb(COM1 + 5) & 0x20))
        __asm__ volatile ("pause");

    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}

static void serial_hex_digit(uint8_t digit)
{
    serial_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
}

void serial_hex8(uint8_t value)
{
    serial_hex_digit((value >> 4) & 0x0F);
    serial_hex_digit(value & 0x0F);
}

void serial_hex16(uint16_t value)
{
    serial_hex8((uint8_t)(value >> 8));
    serial_hex8((uint8_t)value);
}

void serial_hex32(uint32_t value)
{
    serial_hex16((uint16_t)(value >> 16));
    serial_hex16((uint16_t)value);
}
