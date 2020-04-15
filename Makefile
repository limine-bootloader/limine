.PHONY: all clean echfs-test ext2-test

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

echfs-test: all
	$(MAKE) -C test
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	echfs-utils -m -p0 test.img quick-format 32768
	echfs-utils -m -p0 test.img import test/test.elf test.elf
	echfs-utils -m -p0 test.img import test/qloader2.cfg qloader2.cfg
	./qloader2-install src/qloader2.bin test.img
	qemu-system-x86_64 -hda test.img -monitor stdio

ext2-test:
	$(MAKE) -C test
	rm -rf test.img test_image/
	mkdir test_image
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	sudo losetup -Pf --show test.img > loopback_dev
	sudo mkfs.ext2 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo cp test/test.elf test_image
	sudo cp test/qloader2.cfg test_image
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	./qloader2-install src/qloader2.bin test.img
	qemu-system-x86_64 -hda test.img -monitor stdio