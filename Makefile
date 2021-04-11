PREFIX = /usr/local
DESTDIR =

PATH := $(shell pwd)/toolchain/bin:$(PATH)
SHELL := /usr/bin/env bash

TOOLCHAIN = x86_64-elf

TOOLCHAIN_CC = $(TOOLCHAIN)-gcc
TOOLCHAIN_AR = $(TOOLCHAIN)-ar

ifeq ($(shell export "PATH=$(PATH)"; which $(TOOLCHAIN_CC)), )
TOOLCHAIN_CC := gcc
endif
ifeq ($(shell export "PATH=$(PATH)"; which $(TOOLCHAIN_AR)), )
TOOLCHAIN_AR := ar
endif

ifneq ($(shell export "PATH=$(PATH)"; $(TOOLCHAIN_CC) -dumpmachine | head -c 6), x86_64)
$(error No suitable x86_64 GCC compiler found, please install an x86_64 GCC toolchain or run "make toolchain")
endif

STAGE1_FILES := $(shell find -L ./stage1 -type f -name '*.asm' | sort)

.PHONY: all
all:
	$(MAKE) limine-uefi
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install

.PHONY: bin/limine-install
bin/limine-install:
	$(MAKE) -C limine-install LIMINE_HDD_BIN=`realpath bin`/limine-hdd.bin
	[ -f limine-install/limine-install ] && cp limine-install/limine-install bin/ || true
	[ -f limine-install/limine-install.exe ] && cp limine-install/limine-install.exe bin/ || true

.PHONY: clean
clean: limine-bios-clean limine-uefi-clean
	$(MAKE) -C limine-install clean

.PHONY: install
install: all
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -s bin/limine-install "$(DESTDIR)$(PREFIX)/bin/"
	install -d "$(DESTDIR)$(PREFIX)/share"
	install -d "$(DESTDIR)$(PREFIX)/share/limine"
	install -m 644 bin/limine.sys "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 bin/limine-cd.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 bin/limine-eltorito-efi.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 bin/limine-pxe.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 bin/BOOTX64.EFI "$(DESTDIR)$(PREFIX)/share/limine/"

build/stage1: $(STAGE1_FILES) build/decompressor/decompressor.bin build/stage23-bios/stage2.bin.gz
	mkdir -p bin
	cd stage1/hdd && nasm bootsect.asm -Werror -fbin -o ../../bin/limine-hdd.bin
	cd stage1/cd  && nasm bootsect.asm -Werror -fbin -o ../../bin/limine-cd.bin
	cd stage1/pxe && nasm bootsect.asm -Werror -fbin -o ../../bin/limine-pxe.bin
	cp build/stage23-bios/limine.sys ./bin/
	touch build/stage1

.PHONY: limine-bios
limine-bios: stage23-bios decompressor
	$(MAKE) build/stage1

bin/limine-eltorito-efi.bin: build/stage23-uefi/BOOTX64.EFI
	dd if=/dev/zero of=$@ bs=512 count=2880
	mformat -i $@ -f 1440 ::
	mmd -D s -i $@ ::/EFI
	mmd -D s -i $@ ::/EFI/BOOT
	mcopy -D o -i $@ build/stage23-uefi/BOOTX64.EFI ::/EFI/BOOT

.PHONY: limine-uefi
limine-uefi:
	$(MAKE) gnu-efi
	$(MAKE) stage23-uefi
	mkdir -p bin
	cp build/stage23-uefi/BOOTX64.EFI ./bin/
	$(MAKE) bin/limine-eltorito-efi.bin

.PHONY: limine-bios-clean
limine-bios-clean: stage23-bios-clean decompressor-clean

.PHONY: limine-uefi-clean
limine-uefi-clean: stage23-uefi-clean

.PHONY: distclean
distclean: clean test-clean
	rm -rf bin build stivale toolchain ovmf gnu-efi

stivale:
	git clone https://github.com/stivale/stivale.git

.PHONY: stage23-uefi
stage23-uefi: stivale
	$(MAKE) -C stage23 all TARGET=uefi BUILDDIR="`pwd`/build/stage23-uefi"

.PHONY: stage23-uefi-clean
stage23-uefi-clean:
	$(MAKE) -C stage23 clean TARGET=uefi BUILDDIR="`pwd`/build/stage23-uefi"

.PHONY: stage23-bios
stage23-bios: stivale
	$(MAKE) -C stage23 all TARGET=bios BUILDDIR="`pwd`/build/stage23-bios"

.PHONY: stage23-bios-clean
stage23-bios-clean:
	$(MAKE) -C stage23 clean TARGET=bios BUILDDIR="`pwd`/build/stage23-bios"

.PHONY: decompressor
decompressor:
	$(MAKE) -C decompressor all BUILDDIR="`pwd`/build/decompressor"

.PHONY: decompressor-clean
decompressor-clean:
	$(MAKE) -C decompressor clean BUILDDIR="`pwd`/build/decompressor"

.PHONY: test-clean
test-clean:
	$(MAKE) -C test clean
	rm -rf test_image test.hdd test.iso

.PHONY: toolchain
toolchain:
	scripts/make_toolchain.sh "`realpath ./toolchain`" -j`nproc`

gnu-efi:
	git clone https://git.code.sf.net/p/gnu-efi/code --branch=3.0.13 --depth=1 $@
	$(MAKE) -C gnu-efi/gnuefi CC="$(TOOLCHAIN_CC) -m64 -march=x86-64" AR="$(TOOLCHAIN_AR)"
	$(MAKE) -C gnu-efi/lib CC="$(TOOLCHAIN_CC) -m64 -march=x86-64" ARCH=x86_64 x86_64/efi_stub.o

ovmf:
	mkdir -p ovmf
	cd ovmf && wget https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

.PHONY: test.hdd
test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

.PHONY: echfs-test
echfs-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install
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

.PHONY: ext2-test
ext2-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install
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

.PHONY: fat16-test
fat16-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 16 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv bin/* test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	bin/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

.PHONY: fat32-test
fat32-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install
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

.PHONY: iso9660-test
iso9660-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table test_image/ -o test.iso
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

.PHONY: iso9660-uefi-test
iso9660-uefi-test:
	$(MAKE) ovmf
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-uefi
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	xorriso -as mkisofs -eltorito-alt-boot -e boot/limine-eltorito-efi.bin -no-emul-boot test_image/ -o test.iso
	qemu-system-x86_64 -M q35 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

.PHONY: hybrid-iso9660-test
hybrid-iso9660-test:
	$(MAKE) ovmf
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-uefi
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	mkdir -p test_image/EFI/BOOT
	cp -v bin/BOOTX64.EFI test_image/EFI/BOOT/
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e boot/limine-eltorito-efi.bin -no-emul-boot test_image/ -o test.iso
	qemu-system-x86_64 -M q35 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

.PHONY: pxe-test
pxe-test:
	$(MAKE) test-clean
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	qemu-system-x86_64 -enable-kvm -smp 4 -cpu host -netdev user,id=n0,tftp=./test_image,bootfile=boot/limine-pxe.bin -device rtl8139,netdev=n0,mac=00:00:00:11:11:11 -debugcon stdio

.PHONY: uefi-test
uefi-test:
	$(MAKE) ovmf
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-uefi
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
	qemu-system-x86_64 -M q35 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio
