#include <lib/print.h>
#include <lib/real.h>
#include <lib/trace.h>
#include <lib/blib.h>
#include <lib/readline.h>
#include <lib/gterm.h>
#include <mm/pmm.h>

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    asm volatile ("cli" ::: "memory");

    va_list args;

    va_start(args, fmt);

    print("\033[31mPANIC\033[37;1m\033[0m: ");
    vprint(fmt, args);

    va_end(args);

    print("\n");
    print_stacktrace(NULL);

#if defined (bios)
    print("System halted.");
    rm_hcf();
#elif defined (uefi)
    print("Press any key to return to firmware.");
    getchar();
    gterm_clear(true);

    // release all uefi memory and return to firmware
    pmm_release_uefi_mem();
    uefi_call_wrapper(gBS->Exit, 4, efi_image_handle, EFI_ABORTED, 0, NULL);
    __builtin_unreachable();
#endif
}
