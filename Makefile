CC = cc
CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all clean stage2 stage2-clean decompressor decompressor-clean toolchain test.img echfs-test ext2-test fat32-test

all: stage2 decompressor
	gzip -n -9 < stage2/stage2.bin > stage2/stage2.bin.gz
	cd bootsect && nasm bootsect.asm -fbin -o ../limine.bin

clean: stage2-clean decompressor-clean
	rm -f stage2/stage2.bin.gz
	rm -f limine-install
	# We don't remove limine.bin because not everyone wants to build the bootloader themselves.

stage2:
	$(MAKE) -C stage2 all

stage2-clean:
	$(MAKE) -C stage2 clean

decompressor:
	$(MAKE) -C decompressor all

decompressor-clean:
	$(MAKE) -C decompressor clean

toolchain:
	cd toolchain && ./make_toolchain.sh -j`nproc`

limine-install: limine-install.c
	$(CC) $(CFLAGS) limine-install.c -o limine-install

test.img:
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 2048s 100%

echfs-test: limine-install test.img
	$(MAKE) -C test
	echfs-utils -m -p0 test.img quick-format 512
	echfs-utils -m -p0 test.img import test/test.elf boot/test.elf
	echfs-utils -m -p0 test.img import test/limine.cfg limine.cfg
	./limine-install limine.bin test.img
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
	./limine-install limine.bin test.img
	qemu-system-x86_64 -hda test.img -debugcon stdio

fat32-test: limine-install test.img
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.img > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp test/test.elf test_image/boot/
	sudo cp test/limine.cfg test_image/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./limine-install limine.bin test.img
	qemu-system-x86_64 -hda test.img -debugcon stdio
