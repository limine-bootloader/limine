#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <drivers/pic.h>
#include <lib/cio.h>

void pic_eoi(int irq) {
    if (irq >= 8) {
        port_out_b(0xa0, 0x20);
    }

    port_out_b(0x20, 0x20);
}

// Flush all potentially pending IRQs
void pic_flush(void) {
    for (int i = 0; i < 16; i++)
        pic_eoi(i);
}

void pic_set_mask(int line, bool status) {
    uint16_t port;
    uint8_t value;

    if (line < 8) {
        port = 0x21;
    } else {
        port = 0xa1;
        line -= 8;
    }

    if (!status)
        value = port_in_b(port) & ~((uint8_t)1 << line);
    else
        value = port_in_b(port) | ((uint8_t)1 << line);

    port_out_b(port, value);
}

void pic_mask_all(void) {
    port_out_b(0xa1, 0xff);
    port_out_b(0x21, 0xff);
}
