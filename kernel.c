
#include "console.h"
#include "pci.h"
#include "ac97.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR  0x07

static volatile uint16_t *const VGA =
    (volatile uint16_t *)0xB8000;

static size_t cursor = 0;

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c |
           ((uint16_t)color << 8);
}

void console_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA[i] = vga_entry(' ', VGA_COLOR);

    cursor = 0;
}

void console_putc(char c)
{
    if (c == '\n') {
        cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\r') {
        cursor -= cursor % VGA_WIDTH;
    } else {
        if (cursor >= VGA_WIDTH * VGA_HEIGHT)
            cursor = VGA_WIDTH * (VGA_HEIGHT - 1);

        VGA[cursor++] = vga_entry(c, VGA_COLOR);
    }

    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        for (size_t i = 0;
             i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            VGA[i] = VGA[i + VGA_WIDTH];
        }

        for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1);
             i < VGA_WIDTH * VGA_HEIGHT; i++) {
            VGA[i] = vga_entry(' ', VGA_COLOR);
        }

        cursor = VGA_WIDTH * (VGA_HEIGHT - 1);
    }
}

void console_write(const char *s)
{
    while (*s)
        console_putc(*s++);
}

static void console_hex_digit(uint8_t value)
{
    value &= 0x0F;
    console_putc(value < 10
        ? '0' + value
        : 'A' + value - 10);
}

void console_hex8(uint8_t value)
{
    console_hex_digit(value >> 4);
    console_hex_digit(value);
}

void console_hex16(uint16_t value)
{
    console_hex8((uint8_t)(value >> 8));
    console_hex8((uint8_t)value);
}

void console_hex32(uint32_t value)
{
    console_hex16((uint16_t)(value >> 16));
    console_hex16((uint16_t)value);
}

void kernel_main(void)
{

  serial_init();
  serial_write("OS booted\n");
  
    console_clear();
    console_write("MyOS - PCI device enumeration\n");
    console_write("-----------------------------\n\n");

    uint16_t mixer_base;
    uint16_t busmaster_base;
    
    pci_scan(&mixer_base, &busmaster_base);    

    console_write("\nPCI scan complete.\n");

    //    if (ac97_init(0xC000, 0xC400) != 0) {
    if (ac97_init(mixer_base, busmaster_base) != 0) {
      console_write("AC97 initialization failed\n");
    } else {
      ac97_play_test_tone();
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
