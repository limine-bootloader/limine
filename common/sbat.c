#if uefi == 1

#include <config.h>

asm (
    ".section .data.sbat\n\t"
    "sbat:\n\t"
    ".ascii \"sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\\n\"\n\t"
    ".ascii \"limine,1,Limine,limine," LIMINE_VERSION ",https://limine-bootloader.org\\n\"\n\t"
    "__sbat_endv:\n\t"
    ".global __sbat_sizev\n\t"
    ".set __sbat_sizev, __sbat_endv - sbat\n\t"
);

#endif
