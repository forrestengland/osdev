#include "ac97.h"
#include "console.h"
#include "serial.h"
#include "audio.h"

static uint16_t mixer;
static uint16_t busmaster;
static int disable_out_delay = 0;

#define AC97_SAMPLE_RATE  48000

#define AC97_BDL_IOC      0x8000

#define AC97_BUFFER_COUNT  32
#define AC97_BUFFER_FRAMES 256

#define AC97_BUFFER_SAMPLES \
    (AC97_BUFFER_FRAMES * AUDIO_CHANNELS)

struct ac97_bdl_entry {
    uint32_t address;
    uint16_t sample_count;
    uint16_t control;
} __attribute__((packed, aligned(8)));

static struct ac97_bdl_entry bdl[AC97_BUFFER_COUNT]
    __attribute__((aligned(8)));

static int16_t audio_buffers[AC97_BUFFER_COUNT][AC97_BUFFER_SAMPLES]
    __attribute__((aligned(16)));

static uint8_t next_buffer_to_fill;
static int audio_running;

void ac97_pause(unsigned int duration) {
  for (volatile unsigned int i = 0; i < duration; ++i)
    __asm__ volatile ("pause");
}

uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t io_in16(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t io_in32(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
    if (!disable_out_delay) ac97_pause(2000000);      
}

void io_out16(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1"
                      : : "a"(value), "Nd"(port));
    if (!disable_out_delay) ac97_pause(2000000);          
}

void io_out32(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1"
                      : : "a"(value), "Nd"(port));
    if (!disable_out_delay) ac97_pause(2000000);              
}

static void pci_enable_bus_mastering(void)
{
    uint32_t address = 0x80000000 |
                       (0 << 16) |
                       (4 << 11) |
                       (0 << 8) |
                       0x04;

    io_out32(0xCF8, address);

    uint16_t command = io_in16(0xCFC);

    command |= 0x0004;  // Enable bus mastering

    io_out16(0xCFC, command);

    io_out32(0xCF8, address);

    address =
      (1U << 31) |  // Enable
      (0 << 16)  |  // Bus 0
      (31 << 11) |  // Device 31
      (5 << 8)   |  // Function 5
      0x04;         // Command/status register

    io_out32(PCI_CONFIG_ADDRESS, address);

    command = io_in16(PCI_CONFIG_DATA);

    // Enable bus mastering (bit 2).
    command |= (1 << 2);

    io_out32(PCI_CONFIG_ADDRESS, address);
    io_out16(PCI_CONFIG_DATA, command);

    io_out32(PCI_CONFIG_ADDRESS, address);
}

int ac97_init(uint16_t mixer_base, uint16_t busmaster_base)
{

  mixer = mixer_base;
  busmaster = busmaster_base;

  // start audio
  io_out8(busmaster + 0x1B, 0x02);

  pci_enable_bus_mastering();    

  // Assert AC97 cold reset.
  io_out32(busmaster + AC97_GLOB_CNT, 0x00);

  ac97_pause(1000000);

  // Release AC97 cold reset.
  io_out32(busmaster + AC97_GLOB_CNT, 0x02);

  // Wait for primary codec ready.
  int ready = 0;

  for (int i = 0; i < 1000000; i++) {
    uint32_t status = io_in32(busmaster + AC97_GLOB_STA);

    if (status & (1 << 8)) {
      ready = 1;
      break;
    }
  }

  serial_write("Codec ready: ");
  serial_write(ready ? "YES\n" : "NO\n");

  // Headphone volume set
  io_out16(mixer + 0x04, 0x1010);
  ac97_pause(1000000);  
  
  // PCM volume set
  io_out16(mixer + 0x18, 0x1010);
  ac97_pause(1000000);  

  // master volume set
  io_out16(mixer + 0x02, 0x1010);
  ac97_pause(1000000);  

  // Reset the PCM-out bus-master engine.
  // The reset bit is self-clearing.
  io_out8(busmaster + AC97_PO_CR, AC97_CR_RESET);

  unsigned int timeout = 1000000;
  while ((io_in8(busmaster + AC97_PO_CR) & AC97_CR_RESET) &&
	 --timeout) {
    __asm__ volatile ("pause");
  }

  if (timeout == 0) {
    console_write("AC97: PCM-out reset timed out\n");
    return -1;
  }

  return 0;
}

int ac97_start(void)
{
  // Generate initial contents for every buffer.
  for (int i = 0; i < AC97_BUFFER_COUNT; i++) {

    generate_audio(audio_buffers[i], AC97_BUFFER_FRAMES);

    bdl[i].address =
      (uint32_t)(uintptr_t)audio_buffers[i];

    bdl[i].sample_count = AC97_BUFFER_SAMPLES;
    bdl[i].control = AC97_BDL_IOC;
  }

  // Stop PCM-out before programming DMA.
  io_out8(busmaster + AC97_PO_CR, 0);

  // Clear pending status flags.
  io_out16(busmaster + AC97_PO_SR, 0x001C);

  // Reset software buffer tracking.
  next_buffer_to_fill = 0;
  audio_running = 0;

  // Program the descriptor list.
  io_out32(busmaster + AC97_PO_BDBAR,
	   (uint32_t)(uintptr_t)&bdl[0]);

  // All descriptors are initially valid.
  io_out8(busmaster + AC97_PO_LVI,
	  AC97_BUFFER_COUNT - 1);

  // Start PCM-out.
  io_out8(busmaster + AC97_PO_CR, AC97_CR_RUN);

  audio_running = 1;

  console_write("AC97: continuous playback started\n");

  return 0;
}

void ac97_service(void)
{

  disable_out_delay = 1;
  
  if (!audio_running)
    return;

  uint16_t status = io_in16(busmaster + AC97_PO_SR);

  // Check for FIFO error.
  if (status & 0x0010) {
    console_write("AC97: FIFO error\n");
    audio_running = 0;
    io_out8(busmaster + AC97_PO_CR, 0);
    return;
  }

  // No buffer completion yet.
  if (!(status & 0x0008))
    return;

  uint8_t civ = io_in8(busmaster + AC97_PO_CIV);

  // Every descriptor between next_buffer_to_fill and CIV
  // has completed and is safe to refill.
  while (next_buffer_to_fill != civ) {

    uint8_t index = next_buffer_to_fill;

    // Generate new audio in the completed buffer.
    generate_audio(audio_buffers[index], AC97_BUFFER_FRAMES);	

    // Make this descriptor valid again.
    // Advancing LVI allows DMA to continue around the ring.
    io_out8(busmaster + AC97_PO_LVI, (io_in8(busmaster + AC97_PO_LVI) + 1) % AC97_BUFFER_COUNT);	

    next_buffer_to_fill = (next_buffer_to_fill + 1) % AC97_BUFFER_COUNT;
  }

  // Acknowledge buffer completion (write-one-to-clear).
  io_out16(busmaster + AC97_PO_SR, 0x0008);
}
