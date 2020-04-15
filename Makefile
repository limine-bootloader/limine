.PHONY: all clean echfs ext2

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

echfs: all
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

ext2:
	$(MAKE) -C test
	rm -rf test.img test/
	mkdir test
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	sudo losetup --partscan /dev/loop30 test.img
	sudo mkfs.ext2 /dev/loop30p1
	sudo mount /dev/loop30p1 test
	sudo cp test/test.elf test
	sudo cp test/qloader2.cfg test
	sync
	sudo umount test/
	sudo losetup -d /dev/loop30
	rm -rf test
	./qloader2-install src/qloader2.bin test.img
	qemu-system-x86_64 -hda test.img -monitor stdio