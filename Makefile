.PHONY: all clean test

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

test:
	$(MAKE) -C test
	rm -f test.img
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	echfs-utils -m -p0 test.img quick-format 32768
	echfs-utils -m -p0 test.img import test/test.elf test.elf
	echfs-utils -m -p0 test.img import test/qloader2.cfg qloader2.cfg
	./qloader2-install test.img
	qemu-system-x86_64 -hda test.img -monitor stdio

testfs:
	$(MAKE) -C test
	mkdir bruh
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	sudo losetup /dev/loop0 test.img
	sudo mkfs.ext2 /dev/loop0p1
	sudo mount /dev/loop0p1 bruh
	sudo cp test/test.elf bruh
	sudo cp qloader2.cfg bruh
	sudo umount bruh/
	sudo losetup -d /dev/loop0
	rm -rf bruh