ifneq (,)
This makefile requires GNU Make.
endif

PREFIX ?= /usr/local
DESTDIR ?=

BUILDDIR ?= $(shell pwd)/build
override BINDIR := $(BUILDDIR)/bin

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))

override PATH := $(shell pwd)/toolchain/bin:$(PATH)
export PATH

override NCPUS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

override LIMINE_VERSION := $(shell cat version 2>/dev/null || ( git describe --exact-match --tags `git log -n1 --pretty='%h'` 2>/dev/null || git log -n1 --pretty='%h' ) )
export LIMINE_VERSION

override LIMINE_COPYRIGHT := $(shell grep Copyright LICENSE.md)
export LIMINE_COPYRIGHT

TOOLCHAIN ?= limine

TOOLCHAIN_CC ?= $(TOOLCHAIN)-gcc

ifeq ($(shell PATH='$(call SHESCAPE,$(PATH))' command -v $(TOOLCHAIN_CC) ; ), )
override TOOLCHAIN_CC := cc
endif

ifeq ($(TOOLCHAIN_CC), clang)
TOOLCHAIN_CC += --target=x86_64-elf
ifeq ($(TOOLCHAIN_CC), clang)
override TOOLCHAIN_CC += --target=x86_64-elf
MAKEOVERRIDES += TOOLCHAIN_CC+=--target=x86_64-elf
endif
endif

override CC_MACHINE := $(shell PATH='$(call SHESCAPE,$(PATH))' $(TOOLCHAIN_CC) -dumpmachine | dd bs=6 count=1 2>/dev/null)

ifneq ($(MAKECMDGOALS), toolchain)
ifneq ($(MAKECMDGOALS), distclean)
ifneq ($(MAKECMDGOALS), distclean2)
ifneq ($(CC_MACHINE), x86_64)
ifneq ($(CC_MACHINE), amd64-)
$(error No suitable x86_64 C compiler found, please install an x86_64 C toolchain or run "make toolchain")
endif
endif
endif
endif
endif

override STAGE1_FILES := $(shell find -L ./stage1 -type f -name '*.asm')

.PHONY: all
all: limine-uefi limine-uefi32 limine-bios
	$(MAKE) limine-install
	$(MAKE) '$(call SHESCAPE,$(BINDIR))/limine-eltorito-efi.bin'

.PHONY: limine-install
limine-install:
	mkdir -p '$(call SHESCAPE,$(BINDIR))'
	cp limine-install/* limine-install/.gitignore '$(call SHESCAPE,$(BINDIR))/'
	$(MAKE) -C '$(call SHESCAPE,$(BINDIR))'

.PHONY: clean
clean: limine-bios-clean limine-uefi-clean limine-uefi32-clean
	rm -rf '$(call SHESCAPE,$(BINDIR))' '$(call SHESCAPE,$(BUILDDIR))/stage1'

.PHONY: install
install: all
	install -d '$(DESTDIR)$(PREFIX)/bin'
	install -s '$(call SHESCAPE,$(BINDIR))/limine-install' '$(DESTDIR)$(PREFIX)/bin/'
	install -d '$(DESTDIR)$(PREFIX)/share'
	install -d '$(DESTDIR)$(PREFIX)/share/limine'
	install -m 644 '$(call SHESCAPE,$(BINDIR))/limine.sys' '$(DESTDIR)$(PREFIX)/share/limine/' || true
	install -m 644 '$(call SHESCAPE,$(BINDIR))/limine-cd.bin' '$(DESTDIR)$(PREFIX)/share/limine/' || true
	install -m 644 '$(call SHESCAPE,$(BINDIR))/limine-eltorito-efi.bin' '$(DESTDIR)$(PREFIX)/share/limine/' || true
	install -m 644 '$(call SHESCAPE,$(BINDIR))/limine-pxe.bin' '$(DESTDIR)$(PREFIX)/share/limine/' || true
	install -m 644 '$(call SHESCAPE,$(BINDIR))/BOOTX64.EFI' '$(DESTDIR)$(PREFIX)/share/limine/' || true
	install -m 644 '$(call SHESCAPE,$(BINDIR))/BOOTIA32.EFI' '$(DESTDIR)$(PREFIX)/share/limine/' || true

$(call MKESCAPE,$(BUILDDIR))/stage1: $(STAGE1_FILES) $(call MKESCAPE,$(BUILDDIR))/decompressor/decompressor.bin $(call MKESCAPE,$(BUILDDIR))/stage23-bios/stage2.bin.gz
	mkdir -p '$(call SHESCAPE,$(BINDIR))'
	cd stage1/hdd && nasm bootsect.asm -Werror -fbin -DBUILDDIR="'"'$(call SHESCAPE,$(BUILDDIR))'"'" -o '$(call SHESCAPE,$(BINDIR))/limine-hdd.bin'
	cd stage1/cd  && nasm bootsect.asm -Werror -fbin -DBUILDDIR="'"'$(call SHESCAPE,$(BUILDDIR))'"'" -o '$(call SHESCAPE,$(BINDIR))/limine-cd.bin'
	cd stage1/pxe && nasm bootsect.asm -Werror -fbin -DBUILDDIR="'"'$(call SHESCAPE,$(BUILDDIR))'"'" -o '$(call SHESCAPE,$(BINDIR))/limine-pxe.bin'
	cp '$(call SHESCAPE,$(BUILDDIR))/stage23-bios/limine.sys' '$(call SHESCAPE,$(BINDIR))/'
	touch '$(call SHESCAPE,$(BUILDDIR))/stage1'

.PHONY: limine-bios
limine-bios: stage23-bios decompressor
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/stage1'

.PHONY: $(call MKESCAPE,$(BINDIR))/limine-eltorito-efi.bin
$(call MKESCAPE,$(BINDIR))/limine-eltorito-efi.bin:
	mkdir -p '$(call SHESCAPE,$(BINDIR))'
	dd if=/dev/zero of='$(call SHESCAPE,$@)' bs=512 count=2880
	( mformat -i '$(call SHESCAPE,$@)' -f 1440 :: && \
	  mmd -D s -i '$(call SHESCAPE,$@)' ::/EFI && \
	  mmd -D s -i '$(call SHESCAPE,$@)' ::/EFI/BOOT && \
	  ( ( [ -f '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi/BOOTX64.EFI' ] && \
	      mcopy -D o -i '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi/BOOTX64.EFI' ::/EFI/BOOT ) || true ) && \
	  ( ( [ -f '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi32/BOOTIA32.EFI' ] && \
	      mcopy -D o -i '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi32/BOOTIA32.EFI' ::/EFI/BOOT ) || true ) \
	) || rm -f '$(call SHESCAPE,$@)'

.PHONY: limine-uefi
limine-uefi: gnu-efi
	$(MAKE) stage23-uefi
	mkdir -p '$(call SHESCAPE,$(BINDIR))'
	cp '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi/BOOTX64.EFI' '$(call SHESCAPE,$(BINDIR))/'

.PHONY: limine-uefi32
limine-uefi32: gnu-efi
	$(MAKE) stage23-uefi32
	mkdir -p '$(call SHESCAPE,$(BINDIR))'
	cp '$(call SHESCAPE,$(BUILDDIR))/stage23-uefi32/BOOTIA32.EFI' '$(call SHESCAPE,$(BINDIR))/'

.PHONY: limine-bios-clean
limine-bios-clean: stage23-bios-clean decompressor-clean

.PHONY: limine-uefi-clean
limine-uefi-clean: stage23-uefi-clean

.PHONY: limine-uefi32-clean
limine-uefi32-clean: stage23-uefi32-clean

.PHONY: regenerate
regenerate: gnu-efi stivale

.PHONY: dist
dist:
	rm -rf "limine-$(LIMINE_VERSION)"
	LIST="$$(ls -A)"; mkdir "limine-$(LIMINE_VERSION)" && cp -r $$LIST "limine-$(LIMINE_VERSION)/"
	rm -rf "limine-$(LIMINE_VERSION)/"*.tar*
	$(MAKE) -C "limine-$(LIMINE_VERSION)" repoclean
	$(MAKE) -C "limine-$(LIMINE_VERSION)" regenerate
	rm -rf "limine-$(LIMINE_VERSION)/gnu-efi/.git"
	rm -rf "limine-$(LIMINE_VERSION)/stivale/.git"
	rm -rf "limine-$(LIMINE_VERSION)/.git"
	echo "$(LIMINE_VERSION)" > "limine-$(LIMINE_VERSION)/version"
	tar -Jcf "limine-$(LIMINE_VERSION).tar.xz" "limine-$(LIMINE_VERSION)"
	rm -rf "limine-$(LIMINE_VERSION)"

.PHONY: distclean
distclean: clean test-clean
	rm -rf build toolchain ovmf*

.PHONY: repoclean
repoclean: distclean
	rm -rf stivale gnu-efi *.tar.xz

stivale:
	git clone https://github.com/stivale/stivale.git

.PHONY: stage23-uefi
stage23-uefi: stivale
	$(MAKE) -C stage23 all TARGET=uefi BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-uefi'

.PHONY: stage23-uefi-clean
stage23-uefi-clean:
	$(MAKE) -C stage23 clean TARGET=uefi BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-uefi'

.PHONY: stage23-uefi32
stage23-uefi32: stivale
	$(MAKE) -C stage23 all TARGET=uefi32 BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-uefi32'

.PHONY: stage23-uefi32-clean
stage23-uefi32-clean:
	$(MAKE) -C stage23 clean TARGET=uefi32 BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-uefi32'

.PHONY: stage23-bios
stage23-bios: stivale
	$(MAKE) -C stage23 all TARGET=bios BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-bios'

.PHONY: stage23-bios-clean
stage23-bios-clean:
	$(MAKE) -C stage23 clean TARGET=bios BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/stage23-bios'

.PHONY: decompressor
decompressor:
	$(MAKE) -C decompressor all BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/decompressor'

.PHONY: decompressor-clean
decompressor-clean:
	$(MAKE) -C decompressor clean BUILDDIR='$(call SHESCAPE,$(BUILDDIR))/decompressor'

.PHONY: test-clean
test-clean:
	$(MAKE) -C test clean
	rm -rf test_image test.hdd test.iso

.PHONY: toolchain
toolchain:
	MAKE="$(MAKE)" build-aux/make_toolchain.sh "`pwd`/toolchain" -j$(NCPUS)

gnu-efi:
	git clone https://github.com/limine-bootloader/gnu-efi.git --branch=3.0.14 --depth=1
	cp build-aux/elf/* gnu-efi/inc/

ovmf-x64:
	mkdir -p ovmf-x64
	cd ovmf-x64 && curl -o OVMF-X64.zip https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

ovmf-ia32:
	mkdir -p ovmf-ia32
	cd ovmf-ia32 && curl -o OVMF-IA32.zip https://efi.akeo.ie/OVMF/OVMF-IA32.zip && 7z x OVMF-IA32.zip

.PHONY: test.hdd
test.hdd:
	rm -f test.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
	parted -s test.hdd mklabel gpt
	parted -s test.hdd mkpart primary 2048s 100%

.PHONY: echfs-test
echfs-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	echfs-utils -g -p0 test.hdd quick-format 512 > part_guid
	sed "s/@GUID@/`cat part_guid`/g" < test/limine.cfg > limine.cfg.tmp
	echfs-utils -g -p0 test.hdd import limine.cfg.tmp limine.cfg
	rm -f limine.cfg.tmp part_guid
	echfs-utils -g -p0 test.hdd import test/test.elf boot/test.elf
	echfs-utils -g -p0 test.hdd import test/bg.bmp boot/bg.bmp
	echfs-utils -g -p0 test.hdd import $(BINDIR)/limine.sys boot/limine.sys
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: fwcfg-common fwcfg-test fwcfg-simple-test
fwcfg-common:
	$(MAKE) test-clean
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	cp -rv $(BINDIR)/* test_image/boot/
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table test_image/ -o test.iso

fwcfg-simple-test:
	$(MAKE) fwcfg-common
	qemu-system-x86_64 -net none -smp 4   -cdrom test.iso -debugcon stdio \
		-fw_cfg opt/org.limine-bootloader.background,file=test/bg.bmp \
		-fw_cfg opt/org.limine-bootloader.kernel,file=test/test.elf

fwcfg-test:
	$(MAKE) fwcfg-common
	qemu-system-x86_64 -net none -smp 4   -cdrom test.iso -debugcon stdio \
		-fw_cfg opt/org.limine-bootloader.config,file=test/limine-fwcfg.cfg \
		-fw_cfg opt/org.limine-bootloader.background,file=test/bg.bmp \
		-fw_cfg opt/org.limine-bootloader.kernel,file=test/test.elf

.PHONY: ext2-test
ext2-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ext2 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: fat12-test
fat12-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 12 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: fat16-test
fat16-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 16 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: fat32-test
fat32-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: iso9660-test
iso9660-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table test_image/ -o test.iso
	qemu-system-x86_64 -net none -smp 4   -cdrom test.iso -debugcon stdio

.PHONY: ntfs-test
ntfs-test:
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.ntfs `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test/* test_image/boot/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	$(BINDIR)/limine-install test.hdd
	qemu-system-x86_64 -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: full-hybrid-test
full-hybrid-test:
	$(MAKE) ovmf-x64
	$(MAKE) ovmf-ia32
	$(MAKE) test-clean
	$(MAKE) limine-uefi
	$(MAKE) limine-uefi32
	$(MAKE) limine-bios
	$(MAKE) limine-install
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	xorriso -as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine-eltorito-efi.bin -efi-boot-part --efi-boot-image --protective-msdos-label test_image/ -o test.iso
	$(BINDIR)/limine-install test.iso
	qemu-system-x86_64 -M q35 -bios ovmf-x64/OVMF.fd -net none -smp 4   -cdrom test.iso -debugcon stdio
	qemu-system-x86_64 -m 512M -M q35 -bios ovmf-x64/OVMF.fd -net none -smp 4   -hda test.iso -debugcon stdio
	qemu-system-x86_64 -m 512M -M q35 -bios ovmf-ia32/OVMF.fd -net none -smp 4   -cdrom test.iso -debugcon stdio
	qemu-system-x86_64 -m 512M -M q35 -bios ovmf-ia32/OVMF.fd -net none -smp 4   -hda test.iso -debugcon stdio
	qemu-system-x86_64 -m 512M -M q35 -net none -smp 4   -cdrom test.iso -debugcon stdio
	qemu-system-x86_64 -m 512M -M q35 -net none -smp 4   -hda test.iso -debugcon stdio

.PHONY: pxe-test
pxe-test:
	$(MAKE) test-clean
	$(MAKE) limine-bios
	$(MAKE) -C test
	rm -rf test_image/
	mkdir -p test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	qemu-system-x86_64  -smp 4  -netdev user,id=n0,tftp=./test_image,bootfile=boot/limine-pxe.bin -device rtl8139,netdev=n0,mac=00:00:00:11:11:11 -debugcon stdio

.PHONY: uefi-test
uefi-test:
	$(MAKE) ovmf-x64
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
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sudo mkdir -p test_image/EFI/BOOT
	sudo cp $(BINDIR)/BOOTX64.EFI test_image/EFI/BOOT/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	qemu-system-x86_64 -m 512M -M q35 -L ovmf -bios ovmf-x64/OVMF.fd -net none -smp 4   -hda test.hdd -debugcon stdio

.PHONY: uefi32-test
uefi32-test:
	$(MAKE) ovmf-ia32
	$(MAKE) test-clean
	$(MAKE) test.hdd
	$(MAKE) limine-uefi32
	$(MAKE) -C test
	rm -rf test_image/
	mkdir test_image
	sudo losetup -Pf --show test.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat -F 32 `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 test_image
	sudo mkdir test_image/boot
	sudo cp -rv $(BINDIR)/* test_image/boot/
	sudo cp -rv test/* test_image/boot/
	sudo mkdir -p test_image/EFI/BOOT
	sudo cp $(BINDIR)/BOOTIA32.EFI test_image/EFI/BOOT/
	sync
	sudo umount test_image/
	sudo losetup -d `cat loopback_dev`
	rm -rf test_image loopback_dev
	qemu-system-x86_64 -m 512M -M q35 -L ovmf -bios ovmf-ia32/OVMF.fd -net none -smp 4   -hda test.hdd -debugcon stdio
