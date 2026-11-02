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
