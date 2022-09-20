CC ?= cc
INSTALL ?= ./install-sh

PREFIX ?= /usr/local

CFLAGS ?= -g -O2 -pipe -Wall -Wextra

.PHONY: all
all: limine-deploy limine-version

.PHONY: install-data
install-data: all
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

.PHONY: install
install: install-data
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) limine-deploy '$(DESTDIR)$(PREFIX)/bin/'
	$(INSTALL) limine-version '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: install-strip
install-strip: install-data
	$(INSTALL) -d '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) -s limine-deploy '$(DESTDIR)$(PREFIX)/bin/'
	$(INSTALL) -s limine-version '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: clean
clean:
	rm -f limine-deploy limine-deploy.exe
	rm -f limine-version limine-version.exe

limine-deploy: limine-deploy.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -std=c99 -D__USE_MINGW_ANSI_STDIO limine-deploy.c -o $@

limine-version: limine-version.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -std=c99 -D__USE_MINGW_ANSI_STDIO limine-version.c -o $@
