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
        "help      -- Print this help message.\n"
    );
}

void console(void) {
    print("Welcome to the Limine console.\nType 'help' for more information.\n\n");

    static char *prompt = NULL;
    if (prompt == NULL) {
        prompt = ext_mem_alloc(256);
    }

    for (;;) {
        print(">>> ");
        readline("", prompt, 256);

        if (strcmp(prompt, "help") == 0) {
            console_help();
        } else if (strcmp(prompt, "exit") == 0) {
            reset_term();
            return;
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
}
