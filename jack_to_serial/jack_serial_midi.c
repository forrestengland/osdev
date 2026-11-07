
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <termios.h>
#include <time.h>

#define RING_SIZE 65536

static jack_client_t *client;
static jack_port_t *midi_in;
static jack_ringbuffer_t *ring;
static int serial_fd = -1;
static volatile sig_atomic_t running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static int configure_serial(const char *device)
{
    serial_fd = open(device, O_WRONLY | O_NOCTTY);

    if (serial_fd < 0) {
        perror("open serial");
        return -1;
    }

    struct termios tty;

    if (tcgetattr(serial_fd, &tty) < 0) {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&tty);

    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);

    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &tty) < 0) {
        perror("tcsetattr");
        return -1;
    }

    tcflush(serial_fd, TCIOFLUSH);

    return 0;
}

static int process(jack_nframes_t nframes, void *arg)
{
    (void)arg;

    void *buffer = jack_port_get_buffer(midi_in, nframes);

    jack_nframes_t count = jack_midi_get_event_count(buffer);

    for (jack_nframes_t i = 0; i < count; i++) {
        jack_midi_event_t event;

        if (jack_midi_event_get(&event, buffer, i) != 0)
            continue;

        if (jack_ringbuffer_write_space(ring) < event.size)
            continue;

        jack_ringbuffer_write(
            ring,
            (const char *)event.buffer,
            event.size
        );
    }

    return 0;
}

static void *serial_writer(void *arg)
{
    (void)arg;

    uint8_t data[4096];

    while (running) {
        size_t available = jack_ringbuffer_read_space(ring);

        if (available == 0) {
            struct timespec delay = {0, 1000000};
            nanosleep(&delay, NULL);
            continue;
        }

        size_t count = available;

        if (count > sizeof(data))
            count = sizeof(data);

        size_t received = jack_ringbuffer_read(
            ring, (char *)data, count
        );

        size_t offset = 0;

        while (offset < received && running) {
            ssize_t written = write(
                serial_fd,
                data + offset,
                received - offset
            );

            if (written > 0) {
                offset += (size_t)written;
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else {
                perror("serial write");
                running = 0;
                break;
            }
        }
    }

    return NULL;
}

int main(int argc, char **argv)
{
    const char *device = "/dev/ttyUSB0";

    if (argc > 1)
        device = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (configure_serial(device) < 0)
        return 1;

    ring = jack_ringbuffer_create(RING_SIZE);

    if (!ring) {
        fprintf(stderr, "Cannot create MIDI ring buffer\n");
        return 1;
    }

    client = jack_client_open(
        "midi_serial",
        JackNullOption,
        NULL
    );

    if (!client) {
        fprintf(stderr, "Cannot connect to JACK\n");
        return 1;
    }

    midi_in = jack_port_register(
        client,
        "in",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput,
        0
    );

    if (!midi_in) {
        fprintf(stderr, "Cannot register MIDI input\n");
        return 1;
    }

    jack_set_process_callback(client, process, NULL);

    pthread_t writer;

    if (pthread_create(&writer, NULL, serial_writer, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (jack_activate(client) != 0) {
        fprintf(stderr, "Cannot activate JACK client\n");
        running = 0;
        pthread_join(writer, NULL);
        return 1;
    }

    printf("JACK MIDI -> %s at 38400 baud\n", device);
    printf("Connect a MIDI source to midi_serial:in\n");
    printf("Press Ctrl-C to quit.\n");

    while (running)
        sleep(1);

    jack_deactivate(client);
    pthread_join(writer, NULL);

    jack_client_close(client);
    jack_ringbuffer_free(ring);
    close(serial_fd);

    return 0;
}
