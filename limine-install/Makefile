CC ?= cc
INSTALL ?= ./install-sh

PREFIX ?= /usr/local
DESTDIR ?=

CFLAGS ?= -g -O2 -pipe -Wall -Wextra

.PHONY: all
all: limine-install

.PHONY: install-data
install-data: all
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share/limine'
	$(INSTALL) -m 644 limine.sys '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-cd.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-eltorito-efi.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-pxe.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTX64.EFI '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTIA32.EFI '$(DESTDIR)$(PREFIX)/share/limine/'

.PHONY: install
install: install-data
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) limine-install '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: install-strip
install-strip: install-data
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) -s limine-install '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: clean
clean:
	rm -f limine-install limine-install.exe

limine-install: limine-install.c
	$(CC) $(CFLAGS) -std=c11 limine-install.c -o $@
