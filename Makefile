CC ?= cc

PREFIX ?= /usr/local
DESTDIR ?=

CFLAGS ?= -O2 -pipe -Wall -Wextra

.PHONY: all
all: limine-install

.PHONY: install
install: all
	install -d '$(DESTDIR)$(PREFIX)/bin'
	install -s limine-install '$(DESTDIR)$(PREFIX)/bin/'
	install -d '$(DESTDIR)$(PREFIX)/share'
	install -d '$(DESTDIR)$(PREFIX)/share/limine'
	install -m 644 limine.sys '$(DESTDIR)$(PREFIX)/share/limine/'
	install -m 644 limine-cd.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	install -m 644 limine-eltorito-efi.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	install -m 644 limine-pxe.bin '$(DESTDIR)$(PREFIX)/share/limine/'
	install -m 644 BOOTX64.EFI '$(DESTDIR)$(PREFIX)/share/limine/'
	install -m 644 BOOTIA32.EFI '$(DESTDIR)$(PREFIX)/share/limine/'

.PHONY: clean
clean:
	rm -f limine-install limine-install.exe

limine-install: limine-install.c inc.S limine-hdd.bin
	$(CC) $(CFLAGS) -std=c11 limine-install.c inc.S -o $@
