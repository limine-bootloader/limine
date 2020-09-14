DESTDIR =
PREFIX = /usr/local

OS := $(shell uname)
CC = cc
OBJCOPY = objcopy
CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all toolchain stage2 stage2-clean decompressor decompressor-clean limine install clean echfs-test ext2-test fat32-test test.img

all: limine-install

toolchain:
	cd toolchain && ./make_toolchain.sh -j`nproc`

stage2:
	$(MAKE) -C stage2 all

stage2-clean:
	$(MAKE) -C stage2 clean

decompressor:
	$(MAKE) -C decompressor all

decompressor-clean:
	$(MAKE) -C decompressor clean

limine: stage2 decompressor
	gzip -n -9 < stage2/stage2.bin > stage2/stage2.bin.gz
	cd bootsect && nasm bootsect.asm -fbin -o ../limine.bin

clean: stage2-clean decompressor-clean
	rm -f limine-install

limine-install: limine.bin limine-install.c
	$(OBJCOPY) -I binary -O default limine.bin limine.o
	$(CC) $(CFLAGS) limine.o limine-install.c -o limine-install

install: all
	install -s limine-install $(DESTDIR)$(PREFIX)/bin/

test.img:
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
ifeq ($(OS), Linux)
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 2048s 100%
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f test.img -u md9
	sudo gpart create -s mbr md9
	sudo gpart add -a 4k -t '!14' md9
	sudo mdconfig -d -u md9
endif

echfs-test: limine-install test.img
	$(MAKE) -C test
	echfs-utils -m -p0 test.img quick-format 512
	echfs-utils -m -p0 test.img import test/test.elf boot/test.elf
	echfs-utils -m -p0 test.img import test/limine.cfg limine.cfg
	./limine-install test.img
	qemu-system-x86_64 -hda test.img -debugcon stdio -enable-kvm

ext2-test: limine-install test.img
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.img > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ext2 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp test/test.elf test_image/boot/
	sudo cp test/limine.cfg test_image/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./limine-install test.img
	qemu-system-x86_64 -hda test.img -debugcon stdio

fat32-test: limine-install test.img
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
ifeq ($(OS), Linux)
	sudo losetup -Pf --show test.img > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f test.img -u md9
	sudo newfs_msdos -F 32 /dev/md9s1
	sudo mount -t msdosfs /dev/md9s1 test_image
endif
	sudo mkdir test_image/boot
	sudo cp test/test.elf test_image/boot/
	sudo cp test/limine.cfg test_image/
	sync
	sudo umount test_image/
ifeq ($(OS), Linux)
	sudo losetup -d `cat loopback_dev`
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -d -u md9
endif
	rm -rf test_image loopback_dev
	./limine-install test.img
	qemu-system-x86_64 -hda test.img -debugcon stdio
