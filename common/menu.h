#ifndef MENU_H__
#define MENU_H__

#include <stdbool.h>
#include <stdnoreturn.h>
#include <lib/misc.h>

#if defined(UEFI)
bool reboot_to_fw_ui_supported(void);
noreturn void reboot_to_fw_ui(void);
#endif

noreturn void menu(bool first_run);

noreturn void boot(char *config);

char *config_entry_editor(const char *title, const char *orig_entry);

extern no_unwind bool booting_from_editor;

#endif
