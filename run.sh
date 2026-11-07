#!/bin/bash

qemu-system-i386 \
    -machine pc \
    -m 128M \
    -audiodev jack,id=audio0 \
    -device AC97,audiodev=audio0 \
    -device ich9-usb-ehci1,id=ehci \
    -device ich9-usb-uhci1,masterbus=ehci.0,firstport=0,multifunction=on \
    -device ich9-usb-uhci2,masterbus=ehci.0,firstport=2 \
    -device ich9-usb-uhci3,masterbus=ehci.0,firstport=4 \
    -device usb-host,bus=ehci.0,port=1,vendorid=0xfc02,productid=0x0101 \
    -serial stdio \
    -cdrom myos.iso \
    -monitor telnet:127.0.0.1:4444,server,nowait \
    -boot d

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
