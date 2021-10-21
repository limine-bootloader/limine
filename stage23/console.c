#include <stddef.h>
#include <stdint.h>
#include <console.h>
#include <mm/pmm.h>
#include <lib/print.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/term.h>

static void console_help(void) {
    print(
        "Available commands:\n"
        "exit      -- Return to boot menu.\n"
        "version   -- Print version.\n"
        "copyright -- Print copyright.\n"
        "license   -- Print license.\n"
        "help      -- Print this help message.\n"
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
        } else if (strcmp(prompt, "version") == 0) {
            print(LIMINE_VERSION "\n");
        } else if (strcmp(prompt, "copyright") == 0) {
            print(LIMINE_COPYRIGHT "\n");
            print("Limine is distributed under the terms of the BSD-2-Clause license.\n");
        } else if (strcmp(prompt, "license") == 0) {
            print("%s", bsd_2_clause);
        } else if (*prompt != 0) {
            print("Invalid command: `%s`.\n", prompt);
        }
    }

    reset_term();
    pmm_free(prompt, 256);
}
