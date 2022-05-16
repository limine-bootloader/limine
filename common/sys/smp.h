#if defined (__i386__) || defined (__x86_64__)
#include <arch/x86/smp.h>
#elif defined (__aarch64__)
#include <arch/aarch64/smp.h>
#endif