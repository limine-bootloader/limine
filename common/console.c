#include <stddef.h>
#include <stdint.h>
#include <config.h>
#include <console.h>
#include <menu.h>
#include <mm/pmm.h>
#include <lib/print.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <lib/part.h>
#include <lib/config.h>

static void console_help(void) {
    print(
        "Available commands:\n"
        "exit      -- Exit Limine console.\n"
        "clear     -- Clear the console.\n"
        "%s"
        "lsvol     -- List volumes.\n"
        "firmware  -- Show firmware type.\n"
        "version   -- Print version.\n"
        "copyright -- Print copyright.\n"
        "help      -- Print this help message.\n",
        editor_enabled ? "editor    -- Open an empty boot entry editor.\n" : ""
    );
}

void console(void) {
    print("Welcome to the Limine console.\nType 'help' for more information.\n\n");

    char *prompt = ext_mem_alloc(256);

    for (;;) {
        print(">>> ");
        readline("", prompt, 256);

        if (strcmp(prompt, "help") == 0) {
            console_help();
        } else if (strcmp(prompt, "exit") == 0) {
            break;
        } else if (strcmp(prompt, "clear") == 0) {
            print("\e[2J\e[H");
        } else if (strcmp(prompt, "lsvol") == 0) {
            list_volumes();
        } else if (editor_enabled && strcmp(prompt, "editor") == 0) {
            char *new_entry = config_entry_editor("New Entry", "");
            if (new_entry != NULL) {
                config_ready = true;
                boot(new_entry);
            }
        } else if (strcmp(prompt, "firmware") == 0) {
#if defined (BIOS)
            print("BIOS\n");
#elif defined (UEFI)
            print("UEFI\n");
#else
            print("unknown\n");
#endif
        } else if (strcmp(prompt, "version") == 0) {
            print(LIMINE_VERSION "\n");
        } else if (strcmp(prompt, "copyright") == 0) {
            print(LIMINE_COPYRIGHT "\n");
            print("Limine is distributed under the terms of the BSD-2-Clause license.\n");
            print("There is ABSOLUTELY NO WARRANTY, to the extent permitted by law.\n");
        } else if (*prompt != 0) {
            print("Invalid command: `%s`.\n", prompt);
        }
    }

    reset_term();
    pmm_free(prompt, 256);
}
