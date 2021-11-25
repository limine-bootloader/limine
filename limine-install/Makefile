CC = cc

PREFIX = /usr/local
DESTDIR =

BUILDDIR = .
LIMINE_HDD_BIN = limine-hdd.bin

CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all install clean

all:
	$(CC) $(CFLAGS) -std=c11 -DLIMINE_HDD_BIN='"$(LIMINE_HDD_BIN)"' limine-install.c inc.S -o "$(BUILDDIR)/limine-install"

install: all
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -s "$(BUILDDIR)/limine-install" "$(DESTDIR)$(PREFIX)/bin/"
	install -d "$(DESTDIR)$(PREFIX)/share"
	install -d "$(DESTDIR)$(PREFIX)/share/limine"
	install -m 644 limine.sys "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 limine-cd.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 limine-eltorito-efi.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 limine-pxe.bin "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 BOOTX64.EFI "$(DESTDIR)$(PREFIX)/share/limine/"
	install -m 644 BOOTIA32.EFI "$(DESTDIR)$(PREFIX)/share/limine/"

clean:
	rm -f "$(BUILDDIR)/limine-install" "$(BUILDDIR)/limine-install.exe"
