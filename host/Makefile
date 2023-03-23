CC ?= cc
STRIP ?= strip
INSTALL ?= ./install-sh

PREFIX ?= /usr/local

CFLAGS ?= -g -O2 -pipe -Wall -Wextra

.PHONY: all
all: limine-deploy limine-version limine-enroll-config

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
	$(INSTALL) limine-deploy '$(DESTDIR)$(PREFIX)/bin/'
	$(INSTALL) limine-version '$(DESTDIR)$(PREFIX)/bin/'
	$(INSTALL) limine-enroll-config '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: install-strip
install-strip: install
	$(STRIP) '$(DESTDIR)$(PREFIX)/bin/limine-deploy'
	$(STRIP) '$(DESTDIR)$(PREFIX)/bin/limine-version'
	$(STRIP) '$(DESTDIR)$(PREFIX)/bin/limine-enroll-config'

.PHONY: clean
clean:
	rm -f limine-deploy limine-deploy.exe
	rm -f limine-version limine-version.exe
	rm -f limine-enroll-config limine-enroll-config.exe

limine-deploy: limine-deploy.c limine-hdd.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -std=c99 limine-deploy.c $(LIBS) -o $@

limine-version: limine-version.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -std=c99 limine-version.c $(LIBS) -o $@

limine-enroll-config: limine-enroll-config.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -std=c99 limine-enroll-config.c $(LIBS) -o $@
