#if uefi == 1

asm (
    ".section .sbat\n\t"
    ".ascii \"sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\\n\"\n\t"
    ".ascii \"limine,1,Limine,limine," LIMINE_VERSION ",https://limine-bootloader.org\\n\"\n\t"
);

#endif
