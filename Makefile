.PHONY: all clean test

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

test:
	$(MAKE) -C test
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
	parted -s test.img mklabel msdos
	parted -s test.img mkpart primary 1 100%
	echfs-utils -m -p0 test.img quick-format 32768
	echfs-utils -m -p0 test.img import test/test.elf test.elf
	echfs-utils -m -p0 test.img import test/qloader2.cfg qloader2.cfg
	./qloader2-install test.img
	qemu-system-x86_64 -hda test.img
