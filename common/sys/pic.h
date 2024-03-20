#ifndef SYS__PIC_H__
#define SYS__PIC_H__

#include <stdbool.h>

void pic_eoi(int irq);
void pic_flush(void);
void pic_set_mask(int line, bool status);
void pic_mask_all(void);

#endif
