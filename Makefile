
CC = gcc
AS = gcc
LD = ld

CFLAGS = -m32 -std=gnu11 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector

ASFLAGS = -m32 -ffreestanding -fno-pie

all: myos.iso

boot.o: boot.s
	$(AS) $(ASFLAGS) -c $< -o $@

kernel.o: kernel.c console.h pci.h
	$(CC) $(CFLAGS) -c $< -o $@

pci.o: pci.c pci.h console.h
	$(CC) $(CFLAGS) -c $< -o $@

serial.o: serial.c serial.h
	$(CC) $(CFLAGS) -c $< -o $@

ac97.o: ac97.c ac97.h console.h serial.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: boot.o kernel.o pci.o ac97.o serial.o linker.ld
	$(LD) -m elf_i386 -T linker.ld -nostdlib -o $@ boot.o kernel.o pci.o ac97.o serial.o
	@grub-file --is-x86-multiboot $@

myos.iso: kernel.elf iso/boot/grub/grub.cfg
	mkdir -p iso/boot
	cp kernel.elf iso/boot/kernel.elf
	grub-mkrescue -o $@ iso

run: myos.iso
	qemu-system-i386 -cdrom myos.iso -device AC97 -serial stdio

clean:
	rm -f boot.o kernel.o kernel.elf myos.iso ac97.iso
	rm -f iso/boot/kernel.elf

.PHONY: all run clean
