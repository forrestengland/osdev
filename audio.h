#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS    2

void audio_init(void);
void generate_audio(int16_t *buffer, uint32_t frames);

#endif
