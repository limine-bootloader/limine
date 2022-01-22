CC ?= cc
INSTALL ?= ./install-sh

PREFIX ?= /usr/local
DESTDIR ?=

CFLAGS ?= -O2 -pipe -Wall -Wextra

.PHONY: all
all: limine-install

.PHONY: install
install: all
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) -s limine-install '$(DESTDIR)$(PREFIX)/bin/'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share/limine'
	$(INSTALL) -m 644 limine.sys '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-cd.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-eltorito-efi.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-pxe.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTX64.EFI '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTIA32.EFI '$(DESTDIR)$(PREFIX)/share/limine/'

.PHONY: clean
clean:
	rm -f limine-install limine-install.exe

limine-install: limine-install.c inc.S limine-hdd.bin
	$(CC) $(CFLAGS) -std=c11 limine-install.c inc.S -o $@
