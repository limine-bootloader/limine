CC = cc
OBJCOPY = objcopy
CFLAGS = -O2 -pipe -Wall -Wextra
PREFIX = /usr/local
DESTDIR =
TARGET = bios

PATH := $(shell pwd)/toolchain/bin:$(PATH)

.PHONY: all clean install tinf-clean bootloader bootloader-clean distclean stage23 stage23-clean decompressor decompressor-clean toolchain test.hdd echfs-test ext2-test fat32-test iso9660-test

all: bin/limine-install

bin/limine-install: limine-install.c limine-hdd.o
	$(CC) $(CFLAGS) -std=c11 limine-hdd.o limine-install.c -o $@

limine-hdd.o: bin/limine-hdd.bin
	$(OBJCOPY) -B i8086 -I binary -O default bin/limine-hdd.bin $@

clean:
	rm -f limine-hdd.o

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -s limine-install $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/share
	install -m 644 bin/limine.sys $(DESTDIR)$(PREFIX)/share/
	install -m 644 bin/limine-cd.bin $(DESTDIR)$(PREFIX)/share/
	install -m 644 bin/limine-pxe.bin $(DESTDIR)$(PREFIX)/share/

ifeq ($(TARGET), bios)
bootloader: | decompressor stage23
	mkdir -p bin
	cd stage1/hdd && nasm bootsect.asm -fbin -o ../../bin/limine-hdd.bin
	cd stage1/cd  && nasm bootsect.asm -fbin -o ../../bin/limine-cd.bin
	cd stage1/pxe && nasm bootsect.asm -fbin -o ../../bin/limine-pxe.bin
	cp stage23/limine.sys ./bin/
else ifeq ($(TARGET), uefi)
bootloader: | gnu-efi stage23
	mkdir -p bin
	cp stage23/BOOTX64.EFI ./bin/
endif

bootloader-clean: stage23-clean decompressor-clean

distclean: clean bootloader-clean test-clean
	rm -rf bin stivale toolchain ovmf gnu-efi

stivale:
	git clone https://github.com/stivale/stivale.git

stage23: stivale
	cd tinf && rm -rf *.o *.d
	$(MAKE) -C stage23 all TARGET=$(TARGET)

stage23-clean:
	$(MAKE) -C stage23 clean

decompressor:
	cd tinf && rm -rf *.o *.d
	$(MAKE) -C decompressor all

decompressor-clean:
	$(MAKE) -C decompressor clean

test-clean:
	$(MAKE) -C test clean
	rm -rf test_image test.hdd test.iso

toolchain:
	./make_toolchain.sh ./toolchain -j`nproc`

gnu-efi:
	git clone https://git.code.sf.net/p/gnu-efi/code --branch=3.0.12 --depth=1 $@
	$(MAKE) -C gnu-efi

ovmf:
	mkdir -p ovmf
	cd ovmf && wget https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

echfs-test: | test-clean test.hdd bootloader all
	$(MAKE) -C test
	echfs-utils -g -p0 test.hdd quick-format 512 > part_guid
	sed "s/@GUID@/`cat part_guid`/g" < test/limine.cfg > limine.cfg.tmp
	echfs-utils -g -p0 test.hdd import limine.cfg.tmp limine.cfg
	rm -f limine.cfg.tmp part_guid
	echfs-utils -g -p0 test.hdd import test/test.elf boot/test.elf
	echfs-utils -g -p0 test.hdd import test/bg.bmp boot/bg.bmp
	echfs-utils -g -p0 test.hdd import bin/limine.sys boot/limine.sys
	bin/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

ext2-test: | test-clean test.hdd bootloader all
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ext2 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv bin/* test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	bin/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

fat32-test: | test-clean test.hdd bootloader all
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv bin/* test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	bin/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

iso9660-test: | test-clean test.hdd bootloader
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	genisoimage -no-emul-boot -b boot/limine-cd.bin -boot-load-size 4 -boot-info-table -o test.iso test_image/
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

uefi-test: ovmf | test-clean test.hdd bootloader
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv bin/* test/* test_image/boot/
	sudo mkdir -p test_image/EFI/BOOT
	sudo cp bin/BOOTX64.EFI test_image/EFI/BOOT/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	qemu-system-x86_64 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio
