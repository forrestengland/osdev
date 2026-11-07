#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>

int uhci_init(uint16_t io_base);
void uhci_dump_ports(void);
static void uhci_print_port(const char *name, uint16_t value);

#endif
