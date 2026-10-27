
#include <stddef.h>
#include <stdint.h>

static volatile uint16_t *const VGA =
    (volatile uint16_t *)0xB8000;

void kernel_main(void)
{
    const char *message = "Hello from my own OS!";
    const uint8_t color = 0x0A; // Light green on black

    for (size_t i = 0; message[i] != '\0'; i++) {
        VGA[i] = (uint16_t)message[i]
               | ((uint16_t)color << 8);
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
