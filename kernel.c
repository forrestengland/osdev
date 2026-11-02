#include "console.h"
#include "pci.h"
#include "ac97.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

void kernel_main(void) {

  serial_init();
  serial_write("OS booted\n");
  
  console_clear();
  console_write("MyOS - PCI device enumeration\n");
  console_write("-----------------------------\n\n");

  uint16_t mixer_base;
  uint16_t busmaster_base;
    
  pci_scan(&mixer_base, &busmaster_base);    

  console_write("\nPCI scan complete.\n");

  if (ac97_init(mixer_base, busmaster_base) != 0) {
    console_write("AC97 initialization failed\n");
    return;
  }

  if (ac97_start() != 0) {
    console_write("AC97 start failed\n");
    return;
  }

  int counter = 0;
  for (;;) {

    ac97_service();

    //    __asm__ volatile ("hlt");

    counter++;
    
    if (counter >= 1000) {

      counter = 0;

      uint8_t civ = io_in8(busmaster_base + AC97_PO_CIV);
      uint16_t picb = io_in16(busmaster_base + AC97_PO_PICB);
      uint16_t sr = io_in16(busmaster_base + AC97_PO_SR);
      uint8_t cr = io_in8(busmaster_base + AC97_PO_CR);

      serial_write("CIV="); serial_hex8(civ); serial_write(" ");
      serial_write("PICB="); serial_hex16(picb); serial_write(" ");
      serial_write("SR="); serial_hex16(sr); serial_write(" ");
      serial_write("CR="); serial_hex8(cr); serial_write("\n");
    }
  }
}
