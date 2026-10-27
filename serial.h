
#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
void serial_hex8(uint8_t value);
void serial_hex16(uint16_t value);
void serial_hex32(uint32_t value);

#endif
