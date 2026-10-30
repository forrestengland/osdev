
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

void pci_scan(uint16_t* mixer_base, uint16_t* busmaster_base);

#endif
