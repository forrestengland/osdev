#include "ac97.h"
#include "console.h"
#include "serial.h"

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

static uint16_t mixer;
static uint16_t busmaster;

#define AC97_SAMPLE_RATE  48000
#define AC97_TONE_HZ      440
#define AC97_TONE_FRAMES  24000

#define AC97_BDL_IOC      0x8000

struct ac97_bdl_entry {
    uint32_t address;
    uint16_t sample_count;
    uint16_t control;
} __attribute__((packed, aligned(8)));

static struct ac97_bdl_entry bdl[1]
    __attribute__((aligned(8)));

static int16_t tone_buffer[AC97_TONE_FRAMES * 2]
    __attribute__((aligned(16)));

static inline uint8_t io_in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t io_in16(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t io_in32(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_out8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
}

static inline void io_out16(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1"
                      : : "a"(value), "Nd"(port));
}

static inline void io_out32(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1"
                      : : "a"(value), "Nd"(port));
}

int ac97_init(uint16_t mixer_base, uint16_t busmaster_base)
{
    mixer = mixer_base;
    busmaster = busmaster_base;

    /* Bring the AC'97 link into its operational state. */
    io_out32(busmaster + AC97_GLOB_CNT, 0x02);

    /* Wait for the primary codec ready bit (bit 8). */
    unsigned int timeout = 1000000;

    while (!(io_in32(busmaster + AC97_GLOB_STA) & 0x100) &&
	   --timeout) {
      __asm__ volatile ("pause");
    }

    if (timeout == 0) {
      console_write("AC97: codec not ready\n");
      return -1;
    }

    /* Reset mixer registers to their defaults. */
    io_out16(mixer + 0x00, 0x0000);

    /* Set master and PCM output volume; clear mute on both. */
    io_out16(mixer + 0x02, 0x1010);
    io_out16(mixer + 0x18, 0x1010);    

    /*
     * Reset the PCM-out bus-master engine.
     * The reset bit is self-clearing.
     */
    io_out8(busmaster + AC97_PO_CR, AC97_CR_RESET);

    timeout = 1000000;
    while ((io_in8(busmaster + AC97_PO_CR) & AC97_CR_RESET) &&
           --timeout) {
        __asm__ volatile ("pause");
    }

    if (timeout == 0) {
        console_write("AC97: PCM-out reset timed out\n");
        return -1;
    }

    /* Set moderate stereo volume and clear mute. */
    io_out16(mixer + 0x02, 0x1010);

    console_write("AC97: mixer volume = ");
    console_hex16(io_in16(mixer + 0x02));
    console_write("\n");

    console_write("AC97: global status = ");
    console_hex32(io_in32(busmaster + AC97_GLOB_STA));
    console_write("\n");

    console_write("AC97: PCM-out status = ");
    console_hex16(io_in16(busmaster + AC97_PO_SR));
    console_write("\n");

    console_write("AC97 global control: ");
    console_hex32(io_in32(busmaster + AC97_GLOB_CNT));
    console_write("\n");

    console_write("AC97 global status: ");
    console_hex32(io_in32(busmaster + AC97_GLOB_STA));
    console_write("\n");

    console_write("AC97 PCM volume: ");
    console_hex16(io_in16(mixer + 0x18));
    console_write("\n");

    console_write("AC97 master volume: ");
    console_hex16(io_in16(mixer + 0x02));
    console_write("\n");    

    return 0;
}

static void ac97_generate_tone(void)
{
    const int period = AC97_SAMPLE_RATE / AC97_TONE_HZ;

    for (int i = 0; i < AC97_TONE_FRAMES; ++i) {
        int p = i % period;
        int half = period / 2;

        int sample;
        if (p < half) {
            sample = -24000 + (48000 * p) / half;
        } else {
            sample = 24000 -
                     (48000 * (p - half)) / (period - half);
        }

        tone_buffer[i * 2]     = (int16_t)sample;
        tone_buffer[i * 2 + 1] = (int16_t)sample;
    }
}

int ac97_play_test_tone(void)
{
    ac97_generate_tone();

    /*
     * This simple kernel has no paging yet, so linked addresses
     * are expected to be physical addresses. Verify the assumptions
     * in the debugger or linker map if the memory layout changes.
     */
    uintptr_t bdl_addr = (uintptr_t)&bdl[0];
    uintptr_t tone_addr = (uintptr_t)&tone_buffer[0];

    if ((bdl_addr & 7u) != 0 ||
        bdl_addr > 0xFFFFFFFFu ||
        tone_addr > 0xFFFFFFFFu) {
        console_write("AC97: invalid DMA buffer address\n");
        return -1;
    }

    /* One descriptor: 24,000 stereo frames = 48,000 16-bit samples. */
    bdl[0].address = (uint32_t)tone_addr;
    bdl[0].sample_count = AC97_TONE_FRAMES * 2;
    bdl[0].control = AC97_BDL_IOC;

    /* Stop PCM-out before programming its registers. */
    io_out8(busmaster + AC97_PO_CR, 0);


    unsigned int timeout = 1000000;
    while ((io_in8(busmaster + AC97_PO_CR) & AC97_CR_RESET) &&
	   --timeout) {
      __asm__ volatile ("pause");
    }

    if (timeout == 0) {
      console_write("AC97: PCM-out reset timed out\n");
      return -1;
    }
    

    /* Clear pending status flags: write-one-to-clear bits. */
    io_out16(busmaster + AC97_PO_SR, 0x001C);

    /* Program buffer descriptor list and mark descriptor 0 valid. */
    io_out32(busmaster + AC97_PO_BDBAR, (uint32_t)bdl_addr);
    io_out8(busmaster + AC97_PO_LVI, 0);

    console_write("AC97 BDL address: ");
    console_hex32((uint32_t)(uintptr_t)&bdl[0]);
    console_write("\n");

    console_write("AC97 tone address: ");
    console_hex32((uint32_t)(uintptr_t)&tone_buffer[0]);
    console_write("\n");

    console_write("AC97 BDBAR: ");
    console_hex32(io_in32(busmaster + AC97_PO_BDBAR));
    console_write("\n");

    console_write("AC97 LVI: ");
    console_hex8(io_in8(busmaster + AC97_PO_LVI));
    console_write("\n");

    console_write("AC97 CIV: ");
    console_hex8(io_in8(busmaster + AC97_PO_CIV));
    console_write("\n");

    console_write("AC97 PICB: ");
    console_hex16(io_in16(busmaster + AC97_PO_PICB));
    console_write("\n");

    console_write("AC97 status before RUN: ");
    console_hex16(io_in16(busmaster + AC97_PO_SR));
    console_write("\n");

    console_write("AC97 BDL entry address: ");
    console_hex32(bdl[0].address);
    console_write("\n");

    console_write("AC97 BDL sample count: ");
    console_hex16(bdl[0].sample_count);
    console_write("\n");

    console_write("AC97 BDL control: ");
    console_hex16(bdl[0].control);
    console_write("\n");

    serial_write("AC97 global control: ");
    serial_hex32(io_in32(busmaster + AC97_GLOB_CNT));
    serial_write("\n");

    serial_write("AC97 global status: ");
    serial_hex32(io_in32(busmaster + AC97_GLOB_STA));
    serial_write("\n");

    serial_write("AC97 PCM volume: ");
    serial_hex16(io_in16(mixer + 0x18));
    serial_write("\n");

    serial_write("AC97 master volume: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    serial_write("AC97 BDL address: ");
    serial_hex32((uint32_t)(uintptr_t)&bdl[0]);
    serial_write("\n");

    serial_write("AC97 tone address: ");
    serial_hex32((uint32_t)(uintptr_t)&tone_buffer[0]);
    serial_write("\n");

    serial_write("AC97 BDL entry address: ");
    serial_hex32(bdl[0].address);
    serial_write("\n");

    serial_write("AC97 BDL sample count: ");
    serial_hex16(bdl[0].sample_count);
    serial_write("\n");

    serial_write("AC97 BDL control: ");
    serial_hex16(bdl[0].control);
    serial_write("\n");

    serial_write("AC97 BDBAR: ");
    serial_hex32(io_in32(busmaster + AC97_PO_BDBAR));
    serial_write("\n");

    serial_write("AC97 LVI: ");
    serial_hex8(io_in8(busmaster + AC97_PO_LVI));
    serial_write("\n");

    serial_write("AC97 CIV: ");
    serial_hex8(io_in8(busmaster + AC97_PO_CIV));
    serial_write("\n");

    serial_write("AC97 PICB: ");
    serial_hex16(io_in16(busmaster + AC97_PO_PICB));
    serial_write("\n");

    serial_write("AC97 PCM-out status: ");
    serial_hex16(io_in16(busmaster + AC97_PO_SR));
    serial_write("\n");

    serial_write("AC97 PCM-out control: ");
    serial_hex8(io_in8(busmaster + AC97_PO_CR));
    serial_write("\n");    
    

    /* Start PCM-out. */
    io_out16(busmaster + AC97_PO_SR, 0x001C);
    io_out8(busmaster + AC97_PO_CR, AC97_CR_RUN);

    console_write("AC97: tone DMA started\n");

    for (volatile unsigned int i = 0; i < 100000; ++i) {
      __asm__ volatile ("pause");
    }

    console_write("AC97 control after delay: ");
    console_hex8(io_in8(busmaster + AC97_PO_CR));
    console_write("\n");

    console_write("AC97 status after delay: ");
    console_hex16(io_in16(busmaster + AC97_PO_SR));
    console_write("\n");    


    console_write("AC97 control after RUN: ");
    console_hex8(io_in8(busmaster + AC97_PO_CR));
    console_write("\n");    

    /*
     * Wait for descriptor completion or an error.
     * A half-second buffer should finish shortly after playback starts.
     */
    int completed = 0;

    for (unsigned int timeout = 0; timeout < 100000000; ++timeout) {
      uint16_t status = io_in16(busmaster + AC97_PO_SR);

      /* FIFO error: playback did not proceed correctly. */
      if (status & 0x0010) {
        console_write("AC97: FIFO error, status = ");
        console_hex16(status);
        console_write("\n");
        io_out8(busmaster + AC97_PO_CR, 0);
        return -1;
      }

      /* Buffer completion interrupt status. */
      if (status & 0x0008) {
        completed = 1;
        break;
      }
    }

    io_out8(busmaster + AC97_PO_CR, 0);

    if (!completed) {
      console_write("AC97: playback timed out, status = ");
      console_hex16(io_in16(busmaster + AC97_PO_SR));
      console_write("\n");

      serial_write("AC97 global control after timeout: ");
      serial_hex32(io_in32(busmaster + AC97_GLOB_CNT));
      serial_write("\n");

      serial_write("AC97 global status after timeout: ");
      serial_hex32(io_in32(busmaster + AC97_GLOB_STA));
      serial_write("\n");

      serial_write("AC97 PCM volume after timeout: ");
      serial_hex16(io_in16(mixer + 0x18));
      serial_write("\n");

      serial_write("AC97 master volume after timeout: ");
      serial_hex16(io_in16(mixer + 0x02));
      serial_write("\n");

      serial_write("AC97 BDL address after timeout: ");
      serial_hex32((uint32_t)(uintptr_t)&bdl[0]);
      serial_write("\n");

      serial_write("AC97 tone address after timeout: ");
      serial_hex32((uint32_t)(uintptr_t)&tone_buffer[0]);
      serial_write("\n");

      serial_write("AC97 BDL entry address after timeout: ");
      serial_hex32(bdl[0].address);
      serial_write("\n");

      serial_write("AC97 BDL sample count after timeout: ");
      serial_hex16(bdl[0].sample_count);
      serial_write("\n");

      serial_write("AC97 BDL control after timeout: ");
      serial_hex16(bdl[0].control);
      serial_write("\n");

      serial_write("AC97 BDBAR after timeout: ");
      serial_hex32(io_in32(busmaster + AC97_PO_BDBAR));
      serial_write("\n");

      serial_write("AC97 LVI after timeout: ");
      serial_hex8(io_in8(busmaster + AC97_PO_LVI));
      serial_write("\n");

      serial_write("AC97 CIV after timeout: ");
      serial_hex8(io_in8(busmaster + AC97_PO_CIV));
      serial_write("\n");

      serial_write("AC97 PICB after timeout: ");
      serial_hex16(io_in16(busmaster + AC97_PO_PICB));
      serial_write("\n");

      serial_write("AC97 PCM-out status after timeout: ");
      serial_hex16(io_in16(busmaster + AC97_PO_SR));
      serial_write("\n");

      serial_write("AC97 PCM-out control after timeout: ");
      serial_hex8(io_in8(busmaster + AC97_PO_CR));
      serial_write("\n");    
      
      return -1;
    }

    console_write("AC97: playback completed\n");
    return 0;
    
}
