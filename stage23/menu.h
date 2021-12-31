#ifndef __MENU_H__
#define __MENU_H__

#include <stdbool.h>
#include <stdnoreturn.h>

noreturn void menu(bool timeout_enabled);

noreturn void boot(char *config);

char *config_entry_editor(const char *title, const char *orig_entry);

#endif
