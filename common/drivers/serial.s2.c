#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/serial.h>
#include <sys/cpu.h>
#include <lib/blib.h>
#if uefi == 1
#  include <efi.h>
#endif

static bool serial_initialised = false;

#if uefi == 1
static EFI_SERIAL_IO_PROTOCOL *serial_protocol;
#endif

static void serial_initialise(void) {
    if (serial_initialised) {
        return;
    }

#if uefi == 1
    EFI_STATUS status;

    EFI_GUID serial_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

    status = gBS->LocateProtocol(&serial_guid, NULL, (void **)&serial_protocol);
    if (status) {
        return;
    }

    serial_protocol->Reset(serial_protocol);
#endif

#if bios == 1
    // Init com1
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x0c); // 9600 baud
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
#endif

    serial_initialised = true;
}

void serial_out(uint8_t b) {
#if uefi == 1
    if (efi_boot_services_exited) {
        return;
    }
#endif

    serial_initialise();

#if uefi == 1
    UINTN bsize = 1;
    serial_protocol->Write(serial_protocol, &bsize, &b);
#elif bios == 1
    while ((inb(0x3f8 + 5) & 0x20) == 0);
    outb(0x3f8, b);
#endif
}

#if bios == 1
int serial_in(void) {
    serial_initialise();

    if ((inb(0x3f8 + 5) & 0x01) == 0) {
        return -1;
    }
    return inb(0x3f8);
}
#endif
