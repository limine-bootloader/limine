#include <lib/cio.h>
#include <drivers/e9.h>

void e9_write(char c) {
    port_out_b(0xE9, c);
}

int e9_read() {
    return port_in_b(0xE9);
}