#ifndef __LIB__RAND_H__
#define __LIB__RAND_H__

#include <stdint.h>

void init_rand(void);

void srand(uint32_t s);

uint32_t rand32(void);
uint64_t rand64(void);

#endif
