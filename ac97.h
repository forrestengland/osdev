#ifndef AC97_H
#define AC97_H

#include <stdint.h>

int ac97_init(uint16_t mixer_base, uint16_t busmaster_base);
int ac97_play_test_tone(void);

#endif
