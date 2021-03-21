CC = cc
PREFIX = /usr/local
DESTDIR =

PATH := $(shell pwd)/toolchain/bin:$(PATH)

.PHONY: all bin/limine-install clean install distclean limine-bios limine-uefi limine-bios-clean limine-uefi-clean stage23-bios stage23-bios-clean stage23-uefi stage23-uefi-clean decompressor decompressor-clean toolchain test.hdd echfs-test ext2-test fat16-test fat32-test iso9660-test pxe-test uefi-test hybrid-iso9660-test

all:
	$(MAKE) limine-uefi
	$(MAKE) limine-bios
	$(MAKE) bin/limine-install

bin/limine-install:
	$(MAKE) -C limine-install LIMINE_HDD_BIN=`realpath bin`/limine-hdd.bin
	[ -f limine-install/limine-install ] && cp limine-install/limine-install bin/ || true
	[ -f limine-install/limine-install.exe ] && cp limine-install/limine-install.exe bin/ || true

clean: limine-bios-clean limine-uefi-clean
	$(MAKE) -C limine-install clean

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -s bin/limine-install $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/share
	install -d $(DESTDIR)$(PREFIX)/share/limine
	install -m 644 bin/limine.sys $(DESTDIR)$(PREFIX)/share/limine/
	install -m 644 bin/limine-cd.bin $(DESTDIR)$(PREFIX)/share/limine/
	install -m 644 bin/limine-eltorito-efi.bin $(DESTDIR)$(PREFIX)/share/limine/
	install -m 644 bin/limine-pxe.bin $(DESTDIR)$(PREFIX)/share/limine/
	install -m 644 bin/BOOTX64.EFI $(DESTDIR)$(PREFIX)/share/limine/

limine-bios: stage23-bios decompressor
	mkdir -p bin
	cd stage1/hdd && nasm bootsect.asm -fbin -o ../../bin/limine-hdd.bin
	cd stage1/cd  && nasm bootsect.asm -fbin -o ../../bin/limine-cd.bin
	cd stage1/pxe && nasm bootsect.asm -fbin -o ../../bin/limine-pxe.bin
	cp build/stage23-bios/limine.sys ./bin/

bin/limine-eltorito-efi.bin: bin/BOOTX64.EFI
	dd if=/dev/zero of=$@ bs=512 count=2880
	mformat -i $@ -f 1440 ::
	mmd -D s -i $@ ::/EFI
	mmd -D s -i $@ ::/EFI/BOOT
	mcopy -D o -i $@ bin/BOOTX64.EFI ::/EFI/BOOT

limine-uefi:
	$(MAKE) gnu-efi
	$(MAKE) stage23-uefi
	mkdir -p bin
	cp build/stage23-uefi/BOOTX64.EFI ./bin/
	$(MAKE) bin/limine-eltorito-efi.bin

limine-bios-clean: stage23-bios-clean decompressor-clean

limine-uefi-clean: stage23-uefi-clean

distclean: clean test-clean
	rm -rf bin build stivale toolchain ovmf gnu-efi

stivale:
	git clone https://github.com/stivale/stivale.git

stage23-uefi: stivale
	$(MAKE) -C stage23 all TARGET=uefi BUILDDIR="`pwd`/build/stage23-uefi"

stage23-uefi-clean:
	$(MAKE) -C stage23 clean TARGET=uefi BUILDDIR="`pwd`/build/stage23-uefi"

stage23-bios: stivale
	$(MAKE) -C stage23 all TARGET=bios BUILDDIR="`pwd`/build/stage23-bios"

stage23-bios-clean:
	$(MAKE) -C stage23 clean TARGET=bios BUILDDIR="`pwd`/build/stage23-bios"

decompressor:
	$(MAKE) -C decompressor all BUILDDIR="`pwd`/build/decompressor"

decompressor-clean:
	$(MAKE) -C decompressor clean BUILDDIR="`pwd`/build/decompressor"

test-clean:
	$(MAKE) -C test clean
	rm -rf test_image test.hdd test.iso

toolchain:
	$(MAKE) toolchain-bios
	$(MAKE) toolchain-uefi

toolchain-bios:
	scripts/make_toolchain_bios.sh "`realpath ./toolchain`" -j`nproc`

toolchain-uefi:
	scripts/make_toolchain_uefi.sh "`realpath ./toolchain`" -j`nproc`

gnu-efi:
	git clone https://git.code.sf.net/p/gnu-efi/code --branch=3.0.12 --depth=1 $@
	$(MAKE) -C gnu-efi/gnuefi CC=x86_64-elf-gcc AR=x86_64-elf-ar
	$(MAKE) -C gnu-efi/lib CC=x86_64-elf-gcc ARCH=x86_64 x86_64/efi_stub.o

ovmf:
	mkdir -p ovmf
	cd ovmf && wget https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

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

iso9660-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	genisoimage -no-emul-boot -b boot/limine-cd.bin -boot-load-size 4 -boot-info-table -o test.iso test_image/
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

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
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e boot/limine-eltorito-efi.bin -no-emul-boot -isohybrid-gpt-basdat test_image/ -o test.iso
	qemu-system-x86_64 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio

pxe-test:
	$(MAKE) test-clean
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv bin/* test/* test_image/boot/
	qemu-system-x86_64 -enable-kvm -smp 4 -cpu host -netdev user,id=n0,tftp=./test_image,bootfile=boot/limine-pxe.bin -device rtl8139,netdev=n0,mac=00:00:00:11:11:11 -debugcon stdio

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
	qemu-system-x86_64 -L ovmf -bios ovmf/OVMF.fd -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio
