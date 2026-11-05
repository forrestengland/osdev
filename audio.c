#include "audio.h"

#define AC97_BUFFER_SAMPLES			\
    (AC97_BUFFER_FRAMES * AUDIO_CHANNELS)

#define TABLE_SIZE 1024
#define TABLE_MASK (TABLE_SIZE - 1)

#define TWO_PI 6.2831853071795864769f

static int16_t sine_table[TABLE_SIZE];

static uint32_t phase;
static uint32_t phase_increment;

static float frequency = 440.0f;
static float amplitude = 0.25f;

void audio_init(void) {

  // Generate sine wavetable using a rotation recurrence.
    const float sin_step = 0.00613592315f;
    const float cos_step = 0.99998117528f;

    float x = 1.0f;
    float y = 0.0f;

    for (int i = 0; i < TABLE_SIZE; i++) {
        sine_table[i] = (int16_t)(y * 32767.0f);

        float next_x = x * cos_step - y * sin_step;
        float next_y = y * cos_step + x * sin_step;

        x = next_x;
        y = next_y;
    }

    phase = 0;

    phase_increment =
        (uint32_t)(((uint64_t)frequency * 4294967296ULL)
                   / AUDIO_SAMPLE_RATE);
}

void generate_audio(int16_t *buffer, uint32_t frames)
{
    for (uint32_t i = 0; i < frames; i++) {
        int16_t sample = (i & 32) ? 10000 : -10000;

        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}
