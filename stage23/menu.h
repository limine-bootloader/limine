#ifndef __MENU_H__
#define __MENU_H__

#include <stdbool.h>

__attribute__((noreturn))
void menu(bool timeout_enabled);

__attribute__((noreturn))
void boot(char *config);

char *config_entry_editor(const char *title, const char *orig_entry);

#endif
