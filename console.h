
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_clear(void);
void console_write(const char *s);
void console_putc(char c);
void console_hex8(uint8_t value);
void console_hex16(uint16_t value);
void console_hex32(uint32_t value);

#endif
