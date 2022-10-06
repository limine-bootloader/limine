#ifndef __DRIVERS__SERIAL_H__
#define __DRIVERS__SERIAL_H__

#if defined (BIOS)

#include <stdint.h>

void serial_out(uint8_t b);
int serial_in(void);

#endif

#endif
