#ifndef __LIB__TIME_H__
#define __LIB__TIME_H__

#include <stdint.h>

uint64_t time(void);
void bootboot_time(
         uint32_t* day, uint32_t* month, uint32_t* year,
         uint32_t* second, uint32_t* minute, uint32_t* hour);
#endif
