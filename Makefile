CC = cc
OBJCOPY = objcopy
CFLAGS = -O2 -pipe -Wall -Wextra
PREFIX = /usr/local
DESTDIR =

PATH := $(shell pwd)/toolchain/bin:$(PATH)

.PHONY: all clean install tinf-clean bootloader bootloader-clean distclean stages stages-clean decompressor decompressor-clean toolchain test.hdd echfs-test ext2-test fat32-test

all: limine-install

limine-install: limine-install.c limine.o limine_sys.o
	$(CC) $(CFLAGS) -std=c11 limine.o limine_sys.o limine-install.c -o limine-install

limine.o: limine.bin
	$(OBJCOPY) -B i8086 -I binary -O default limine.bin limine.o

limine_sys.o: limine.bin
	$(OBJCOPY) -B i8086 -I binary -O default limine.sys limine_sys.o

clean:
	rm -f limine.o limine_sys.o limine-install

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -s limine-install $(DESTDIR)$(PREFIX)/bin/

bootloader: | decompressor stages
	cd hddboot && nasm bootsect.asm -fbin -o ../limine-hdd.bin
	cd cdboot && nasm bootsect.asm -fbin -o ../limine-cd.bin
	cd pxeboot && nasm bootsect.asm -fbin -o ../limine-pxe.bin
	cp stages/stages.map ./
	cp stages/stage3.bin ./limine.sys
	cp stages/stages.bin ./

bootloader-clean: stages-clean decompressor-clean test-clean
	rm -f test/stages.map test.hdd

distclean: clean bootloader-clean
	rm -rf stivale

tinf-clean:
	cd tinf && rm -rf *.o *.d

stivale:
	git clone https://github.com/stivale/stivale.git
	cd stivale && git checkout d0a7ca5642d89654f8d688c2481c2771a8653c99

stages: tinf-clean stivale
	$(MAKE) -C stages all

stages-clean:
	$(MAKE) -C stages clean

decompressor: tinf-clean
	$(MAKE) -C decompressor all

decompressor-clean:
	$(MAKE) -C decompressor clean

test-clean:
	$(MAKE) -C test clean

toolchain:
	cd toolchain && ./make_toolchain.sh -j`nproc`

test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

echfs-test: test.hdd bootloader | all
	$(MAKE) -C test
	echfs-utils -g -p0 test.hdd quick-format 512 > part_guid
	sed "s/@GUID@/`cat part_guid`/g" < test/limine.cfg > limine.cfg.tmp
	echfs-utils -g -p0 test.hdd import limine.cfg.tmp limine.cfg
	rm -f limine.cfg.tmp part_guid
	echfs-utils -g -p0 test.hdd import stages.map boot/stages.map
	echfs-utils -g -p0 test.hdd import test/test.elf boot/test.elf
	echfs-utils -g -p0 test.hdd import test/bg.bmp boot/bg.bmp
	./limine-install ./ test.hdd
	echfs-utils -g -p0 test.hdd import ./limine.sys boot/limine.sys
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

ext2-test: test.hdd bootloader | all
	$(MAKE) -C test
	cp stages.map test/
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ext2 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv ./limine.sys test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./limine-install ./ test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

fat32-test: test.hdd bootloader | all
	$(MAKE) -C test
	cp stages.map test/
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv ./limine.sys test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./limine-install ./ test.hdd
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -hda test.hdd -debugcon stdio

iso9660-test: bootloader
	$(MAKE) -C test
	cp stages.map test/
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv limine-cd.bin stages.bin test/* test_image/boot/
	genisoimage -no-emul-boot -b boot/limine-cd.bin -o test.iso test_image/
	qemu-system-x86_64 -net none -smp 4 -enable-kvm -cpu host -cdrom test.iso -debugcon stdio
