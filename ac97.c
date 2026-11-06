#include "ac97.h"
#include "console.h"
#include "serial.h"
#include "audio.h"

static uint16_t mixer;
static uint16_t busmaster;

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
}

void io_out16(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1"
                      : : "a"(value), "Nd"(port));
}

void io_out32(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1"
                      : : "a"(value), "Nd"(port));
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

  for (volatile unsigned int i = 0; i < 1000000; ++i)
    __asm__ volatile ("pause");

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

  // Headphone register test
  io_out16(mixer + 0x04, 0x1F1F);

  io_out16(mixer + 0x04, 0x1010);
  
  // PCM register test
  io_out16(mixer + 0x18, 0x1F1F);

  /* -- */
  serial_write(", PCM test 1F1F:");
  serial_hex16(io_in16(mixer + 0x18));
  /* -- */

  io_out16(mixer + 0x18, 0x1010);

  /* --- */
  serial_write(", PCM test 0000: ");
  serial_hex16(io_in16(mixer + 0x18));
  /* --- */

    uint16_t original = io_in16(mixer + 0x02);

    serial_write("Master original: ");
    serial_hex16(original);
    serial_write("\n");

    io_out16(mixer + 0x02, 0x1F1F);

    for (volatile unsigned int i = 0; i < 100000; i++)
      __asm__ volatile ("pause");

    serial_write("Master test 1F1F: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    io_out16(mixer + 0x02, 0x0000);

    for (volatile unsigned int i = 0; i < 100000; i++)
      __asm__ volatile ("pause");

    serial_write("Master test 0000: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");    

    // Power up the codec analog sections.
    io_out16(mixer + 0x26, 0x0000);

    // Give the codec time to stabilize.
    for (volatile unsigned int i = 0; i < 1000000; i++) {
      __asm__ volatile ("pause");
    }

    serial_write("Power management after wake: ");
    serial_hex16(io_in16(mixer + 0x26));
    serial_write("\n");

    // Unmute and set maximum output volume.
    io_out16(mixer + 0x02, 0x0000);  // Master
    io_out16(mixer + 0x04, 0x0000);  // Headphone
    io_out16(mixer + 0x18, 0x0000);  // PCM

    for (volatile unsigned int i = 0; i < 100000; i++) {
      __asm__ volatile ("pause");
    }

    serial_write("Master: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    serial_write("Headphone: ");
    serial_hex16(io_in16(mixer + 0x04));
    serial_write("\n");

    serial_write("PCM: ");
    serial_hex16(io_in16(mixer + 0x18));
    serial_write("\n"); 

    serial_write("Codec reset register: ");
    serial_hex16(io_in16(mixer + 0x00));
    serial_write("\n");

    serial_write("Codec vendor ID 1: ");
    serial_hex16(io_in16(mixer + 0x7C));
    serial_write("\n");

    serial_write("Codec vendor ID 2: ");
    serial_hex16(io_in16(mixer + 0x7E));
    serial_write("\n");

    serial_write("Codec master capabilities: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    serial_write("Codec PCM volume: ");
    serial_hex16(io_in16(mixer + 0x18));
    serial_write("\n");

    for (volatile unsigned int i = 0; i < 10000; i++) {
      __asm__ volatile ("pause");
    }

    serial_write("Master volume readback: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    serial_write("PCM volume readback: ");
    serial_hex16(io_in16(mixer + 0x18));
    serial_write("\n");    

    // Read back both registers.
    serial_write("Master volume after write: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write("\n");

    serial_write("PCM volume after write: ");
    serial_hex16(io_in16(mixer + 0x18));
    serial_write("\n");    

    serial_write("AC97 global control after reset: ");
    serial_hex32(io_in32(busmaster + AC97_GLOB_CNT));
    serial_write("\n");

    serial_write("AC97 global status after reset: ");
    serial_hex32(io_in32(busmaster + AC97_GLOB_STA));
    serial_write("\n");    

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

    serial_write("Codec power management: ");
    serial_hex16(io_in16(mixer + 0x26));
    serial_write("\n");

    // Configure AD1981B output levels.
    io_out16(mixer + 0x02, 0x0000);  // Master volume
    io_out16(mixer + 0x04, 0x0000);  // Headphone volume
    io_out16(mixer + 0x18, 0x0000);  // PCM volume

    // Allow codec time to process writes.
    for (volatile unsigned int i = 0; i < 100000; i++) {
      __asm__ volatile ("pause");
    }

    uint16_t master = io_in16(mixer + 0x02);
    uint16_t headphone = io_in16(mixer + 0x04);
    uint16_t pcm = io_in16(mixer + 0x18);

    serial_write("Final master volume: ");
    serial_hex16(master);
    serial_write("\n");

    serial_write("Final headphone volume: ");
    serial_hex16(headphone);
    serial_write("\n");

    serial_write("Final PCM volume: ");
    serial_hex16(pcm);
    serial_write("\n");    

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

    io_out16(mixer + 0x02, 0x0000); // Master volume, unmuted
    io_out16(mixer + 0x04, 0x0000); // Headphone volume, unmuted
    io_out16(mixer + 0x18, 0x0000); // PCM volume, unmuted

    serial_write("Master after final setup: ");
    serial_hex16(io_in16(mixer + 0x02));
    serial_write(", Headphone after final setup: ");
    serial_hex16(io_in16(mixer + 0x04));
    serial_write(", PCM after final setup: ");
    serial_hex16(io_in16(mixer + 0x18));

    // Start PCM-out.
    io_out8(busmaster + AC97_PO_CR, AC97_CR_RUN);

    audio_running = 1;

    console_write("AC97: continuous playback started\n");

    return 0;
}

void ac97_service(void)
{
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
