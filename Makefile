.PHONY: all clean echfs-test ext2-test

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

echfs-test: all
	$(MAKE) -C test
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel gpt
	parted -s test.img mkpart primary 2048s 6143s
	parted -s test.img mkpart primary 6144s 131038s
	echfs-utils -g -p1 test.img quick-format 512
	echfs-utils -g -p1 test.img import test/test.elf boot/test.elf
	echfs-utils -g -p1 test.img import test/qloader2.cfg qloader2.cfg
	./qloader2-install src/qloader2.bin test.img 2048
	qemu-system-x86_64 -hda test.img -debugcon stdio

ext2-test: all
	$(MAKE) -C test
	rm -rf test.img test_image/
	mkdir test_image
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel gpt
	parted -s test.img mkpart primary 2048s 6143s
	parted -s test.img mkpart primary 6144s 131038s
	sudo losetup -Pf --show test.img > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ext2 `cat loopback_dev`p2
	sudo mount `cat loopback_dev`p2 test_image
	sudo mkdir test_image/boot
	sudo cp test/test.elf test_image/boot/
	sudo cp test/qloader2.cfg test_image/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./qloader2-install src/qloader2.bin test.img 2048
	qemu-system-x86_64 -hda test.img -debugcon stdio

fat32-test: all
	$(MAKE) -C test
	rm -rf test.img test_image/
	mkdir test_image
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel gpt
	parted -s test.img mkpart primary 2048s 6143s
	parted -s test.img mkpart primary 6144s 131038s
	sudo losetup -Pf --show test.img > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p2
	sudo mount `cat loopback_dev`p2 test_image
	sudo mkdir test_image/boot
	sudo cp test/test.elf test_image/boot/
	sudo cp test/qloader2.cfg test_image/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./qloader2-install src/qloader2.bin test.img 2048
	qemu-system-x86_64 -hda test.img -debugcon stdio
