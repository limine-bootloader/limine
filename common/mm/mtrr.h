#ifndef __MM__MTRR_H__
#define __MM__MTRR_H__

#if defined (__x86_64__) || defined (__i386__)

void mtrr_save(void);
void mtrr_restore(void);

#endif

#endif
