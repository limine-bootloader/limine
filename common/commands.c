#include <commands.h>
#include <config.h>
#include <menu.h>
#include <lib/print.h>
#include <lib/misc.h>
#include <lib/config.h>

void print_firmware(void) {
#if defined (BIOS)
    print("BIOS\n");
#elif defined (UEFI)
    print("UEFI\n");
#else
    print("unknown\n");
#endif
}

void clear_console(void) {
    print("\e[2J\e[H");
}

#if defined (UEFI)
extern symbol __slide;
void print_load_slide_offset(void) {
    print("%p\n", __slide);
}

void firmware_setup() {
    if (reboot_to_fw_ui_supported()) {
        reboot_to_fw_ui();
    } else {
        print("Your firmware does not support rebooting to the firmware UI.\n");
    }
}
#endif

void print_version(void) {
    print(LIMINE_VERSION "\n");
}

void print_copyright(void) {
    print(LIMINE_COPYRIGHT "\n");
    print("Limine is distributed under the terms of the BSD-2-Clause license.\n");
    print("There is ABSOLUTELY NO WARRANTY, to the extent permitted by law.\n");
}

void open_editor(void) {
    char *new_entry = config_entry_editor("New Entry", "");
    if (new_entry != NULL) {
        config_ready = true;
        boot(new_entry);
    }
}
