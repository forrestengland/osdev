#include "console.h"
#include "pci.h"
#include "ac97.h"
#include "serial.h"
#include "uhci.h"

#include <stddef.h>
#include <stdint.h>

static void midi_serial_test(void)
{
    uint8_t byte;

    while (serial_try_read(&byte)) {
        console_write("MIDI: ");
        console_hex8(byte);
        console_write("\n");
    }
}

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

  /* init usb for midi */
  uhci_init(0xC540);
  uhci_dump_ports();
  

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

    midi_serial_test();

    counter++;
    
    if (counter >= 1000) {

      counter = 0;

    }
  }
}
