gcc -Wall -Wextra -O2 jack_serial_midi.c \
    -o jack_serial_midi \
    $(pkg-config --cflags --libs jack) -pthread
