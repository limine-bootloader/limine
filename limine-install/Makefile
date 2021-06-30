CC = cc
OBJCOPY = objcopy

PREFIX = /usr/local
DESTDIR =

LIMINE_HDD_BIN = limine-hdd.bin
BUILD_DIR = $(shell realpath .)

CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all clean

all: limine-install

install: all
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -s limine-install "$(DESTDIR)$(PREFIX)/bin/"
	install -d "$(DESTDIR)$(PREFIX)/share"
	install -d "$(DESTDIR)$(PREFIX)/share/limine"
	install -m 644 limine.sys "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 limine-cd.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 limine-eltorito-efi.bin "$(DESTDIR)$(PREFIX)/share/limine/" || true
	install -m 644 limine-pxe.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 BOOTX64.EFI "$(DESTDIR)$(PREFIX)/share/limine/"

clean:
	rm -f limine-install limine-install.exe

limine-install: limine-install.c inc.S $(LIMINE_HDD_BIN)
	$(CC) $(CFLAGS) -std=c11 -DLIMINE_HDD_BIN='"$(LIMINE_HDD_BIN)"' limine-install.c inc.S -o $@
