#!/bin/bash

qemu-system-i386 \
    -machine pc \
    -m 128M \
    -audiodev jack,id=audio0 \
    -device AC97,audiodev=audio0 \
    -serial stdio \
    -cdrom myos.iso \
    -boot d &

QEMU_PID=$!

# Wait for QEMU's JACK output ports to appear.
echo 'waiting to connect ports'
for i in $(seq 1 50); do
    PORTS=$(jack_lsp 2>/dev/null | grep '^qemu-system-i386:')

    if [ -n "$PORTS" ]; then
        break
    fi

    sleep 0.1
done

# Connect each QEMU output to the corresponding system playback port.
echo 'connecting ports'
echo "$PORTS" | while read -r port; do
    case "$port" in
        *out_0*) jack_connect "$port" system:playback_1 ;;
        *out_1*) jack_connect "$port" system:playback_2 ;;
    esac
done

wait "$QEMU_PID"
