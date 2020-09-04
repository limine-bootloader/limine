DESTDIR =
PREFIX = /usr/local

OS := $(shell uname)
CC = clang
OBJCOPY = llvm-objcopy
CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all install clean echfs-test ext2-test test.img

all: limine-install

install: all
	install -s limine-install $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f limine-install
	$(MAKE) -C src clean

src/limine.bin:
	$(MAKE) -C src all

limine-install: src/limine.bin limine-install.c
	$(CC) $(CFLAGS) -c limine-install.c -o limine-install.o
	# FIXME: GNU objcopy supports `-O default` but for some stupid reason
	#        llvm-objcopy does not. This needs to be worked around.
	#        For now hardcode elf64-x86-64.
	$(OBJCOPY) -I binary -O elf64-x86-64 src/limine.bin limine.o
	$(CC) $(CFLAGS) limine.o limine-install.o -o limine-install

test.img:
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=4096 of=test.img
ifeq ($(OS), Linux)
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 0% 100%
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f test.img -u md9
	sudo gpart create -s mbr md9
	sudo gpart add -a 4k -t '!14' md9
	sudo mdconfig -d -u md9
endif

echfs-test: limine-install test.img
	$(MAKE) -C test
	echfs-utils -m -p0 test.img quick-format 32768
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
	sudo cp test/916768.bmp test_image/
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
