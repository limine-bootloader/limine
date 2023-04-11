CC ?= cc
STRIP ?= strip
INSTALL ?= ./install-sh

PREFIX ?= /usr/local

CFLAGS ?= -g -O2 -pipe

.PHONY: all
all: limine

.PHONY: install
install: all
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/share/limine'
	$(INSTALL) -m 644 limine.sys '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-cd.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-cd-efi.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 limine-pxe.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTX64.EFI '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -m 644 BOOTIA32.EFI '$(DESTDIR)$(PREFIX)/share/limine/'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/include'
	$(INSTALL) -m 644 limine.h '$(DESTDIR)$(PREFIX)/include/'
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) limine '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: install-strip
install-strip: install
	$(STRIP) '$(DESTDIR)$(PREFIX)/bin/limine'

.PHONY: clean
clean:
	rm -f limine limine.exe

limine: limine.c
	$(CC) $(CFLAGS) -Wall -Wextra $(WERROR_FLAG) $(CPPFLAGS) $(LDFLAGS) -std=c99 limine.c $(LIBS) -o $@
