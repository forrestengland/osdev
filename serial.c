
#include "serial.h"

#define DISABLE_SERIAL_OUTPUT

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

#define UART_DATA       0
#define UART_IER        1
#define UART_FCR        2
#define UART_LCR        3
#define UART_MCR        4
#define UART_LSR        5

#define UART_LSR_DATA_READY  0x01

void serial_init(void)
{
    /* Disable UART interrupts. */
    outb(COM1 + UART_IER, 0x00);

    /* Enable divisor latch. */
    outb(COM1 + UART_LCR, 0x80);

    /* 38400 baud: 115200 / 3. */
    outb(COM1 + UART_DATA, 0x03);
    outb(COM1 + UART_IER, 0x00);

    /* 8 data bits, no parity, 1 stop bit. */
    outb(COM1 + UART_LCR, 0x03);

    /* Enable FIFO, clear RX/TX FIFO, 14-byte threshold. */
    outb(COM1 + UART_FCR, 0xC7);

    /* DTR, RTS, OUT2. */
    outb(COM1 + UART_MCR, 0x0B);
}

void serial_putc(char c)
{

#ifdef DISABLE_SERIAL_OUTPUT
    return;
#endif

    while (!(inb(COM1 + UART_LSR) & 0x20))
        __asm__ volatile ("pause");

    outb(COM1 + UART_DATA, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');

        serial_putc(*s++);
    }
}

/* Return nonzero when a byte is available. */
int serial_rx_ready(void)
{
    return (inb(COM1 + UART_LSR) & UART_LSR_DATA_READY) != 0;
}

/* Read one byte without blocking. */
int serial_try_read(uint8_t *value)
{
    if (!serial_rx_ready())
        return 0;

    *value = inb(COM1 + UART_DATA);
    return 1;
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
