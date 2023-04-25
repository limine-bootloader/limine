#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/serial.h>
#include <sys/cpu.h>
#include <lib/misc.h>
#include <lib/print.h>

bool serial = false;
int serial_baudrate = 9600;

static bool serial_initialised = false;
static bool uart_is_16550 = false;

#if defined(UEFI)
EFI_SERIAL_IO_PROTOCOL *serial_io_protocol;
#endif

// In theory this could be used anywhere that implements a 16550 UART, but would need to be adapted to MMIO
static void uart16550_initialise(void) {
    if (serial_initialised) {
        return;
    }

#if defined(__i386__) || defined(__x86_64__)
    // Init com1
    outb(0x3f8 + 3, 0x00);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);

    uint16_t divisor =  (uint16_t)(115200 / serial_baudrate);
    outb(0x3f8 + 0, divisor & 0xff);
    outb(0x3f8 + 1, (divisor >> 8) & 0xff);

    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
#endif

    serial_initialised = true;
    uart_is_16550 = true;
}

static void serial_initialise(void) {
    if (serial_initialised) {
        return;
    }

#if defined(UEFI)
    EFI_STATUS status;
    EFI_GUID serial_io_protocol_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

    status = gBS->LocateProtocol(&serial_io_protocol_guid, NULL, (void **)&serial_io_protocol);
    if (status) {
        uart16550_initialise();
        print("Could not get serial io protocol, defaulting to 16550 style uart\n");
        return;
    }

	// 0 indicates hardware defaults
    status = serial_io_protocol->SetAttributes(serial_io_protocol, serial_baudrate, 0, 0, 0, 0, 0);
    if (status) {
        panic(false, "Could not set serial attributes");
    }

    status = serial_io_protocol->Reset(serial_io_protocol);
    if (status) {
        panic(false, "Could not reset serial io protocol");
    }

    serial_initialised = true;
    return;
#else
    uart16550_initialise();
#endif
}

void serial_out(uint8_t b) {
    serial_initialise();

#if defined(UEFI)
    if (!uart_is_16550) {
        uint8_t buffer[] = {b, 0};
        UINTN buffer_size = 1;
        EFI_STATUS ret = serial_io_protocol->Write(serial_io_protocol, &buffer_size, buffer);
        if (ret) {
            panic(false, "Could not write to serial port");
        }
        return;
    }
#endif

#if defined(__i386__) || defined(__x86_64__)
    while ((inb(0x3f8 + 5) & 0x20) == 0);
    outb(0x3f8, b);
#endif
}

int serial_in(void) {
    serial_initialise();

#if defined(UEFI)
    if (!uart_is_16550) {
        EFI_STATUS ret;
        uint32_t status;
        ret = serial_io_protocol->GetControl(serial_io_protocol, &status);
        if (ret) {
            panic(false, "Could not get serial control");
        }

        if ((status & EFI_SERIAL_INPUT_BUFFER_EMPTY) != 0) {
            return -1;
        }

        uint8_t buffer[1];
        UINTN buffer_size = 1;
        ret = serial_io_protocol->Read(serial_io_protocol, &buffer_size, buffer);
        if (ret) {
            panic(false, "Could not read from serial port");
        }

        return buffer[0];
    }
#endif

#if defined(__i386__) || defined(__x86_64__)
    if ((inb(0x3f8 + 5) & 0x01) == 0) {
        return -1;
    }
    return inb(0x3f8);
#endif

    return -1;
}
