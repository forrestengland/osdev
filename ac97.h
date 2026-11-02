#ifndef AC97_H
#define AC97_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define AC97_PO_BDBAR  0x10
#define AC97_PO_CIV    0x14
#define AC97_PO_LVI    0x15
#define AC97_PO_SR     0x16
#define AC97_PO_PICB   0x18
#define AC97_PO_CR     0x1B

#define AC97_GLOB_CNT  0x2C
#define AC97_GLOB_STA  0x30

#define AC97_CR_RUN    0x01
#define AC97_CR_RESET  0x02

uint8_t io_in8(uint16_t port);
uint16_t io_in16(uint16_t port);
uint32_t io_in32(uint16_t port);
void io_out8(uint16_t port, uint8_t value);
void io_out16(uint16_t port, uint16_t value);
void io_out32(uint16_t port, uint32_t value);

int ac97_init(uint16_t mixer_base, uint16_t busmaster_base);
//int ac97_play_test_tone(void);
int ac97_start(void);
void ac97_service(void);

#endif
