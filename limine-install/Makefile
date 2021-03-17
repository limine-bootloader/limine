OBJCOPY = objcopy
OBJCOPY_ARCH = default
LIMINE_HDD_BIN = limine-hdd.bin
BUILD_DIR = $(shell realpath .)

CFLAGS = -O2 -pipe -Wall -Wextra

.PHONY: all clean

all: limine-install

clean:
	rm -f limine-hdd.o limine-install

limine-install: limine-install.c limine-hdd.o
	$(CC) $(CFLAGS) -std=c11 $^ -o $@

limine-hdd.o: $(LIMINE_HDD_BIN)
	cd `dirname $^` && \
	  $(OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) `basename $^` $(BUILD_DIR)/$@
