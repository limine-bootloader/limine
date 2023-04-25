#ifndef __DRIVERS__SERIAL_H__
#define __DRIVERS__SERIAL_H__

#include <stdbool.h>
#include <stdint.h>

extern bool serial;
extern int serial_baudrate;

void serial_out(uint8_t b);
int serial_in(void);

#endif
