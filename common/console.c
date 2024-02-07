#include <stddef.h>
#include <stdint.h>
#include <commands.h>
#include <console.h>
#include <mm/pmm.h>
#include <lib/print.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <lib/part.h>
#include <lib/config.h>

static void console_help(void);
bool console_mode;

static void exit_console(void) {
    console_mode = false;
}

struct Command {
    char *name;
    void (*run_command)(void);
    char *description;
};

const struct Command commands[] = {
    {"exit",&exit_console,"Exit Limine console."},
    {"clear",&clear_console,"Clear the console."},
    {"lsvol",&list_volumes,"List volumes."},
    {"firmware",&print_firmware,"Show firmware type."},
#if defined (UEFI)
    {"slide",&print_load_slide_offset,"Print load slide offset."},
    {"setup",&firmware_setup,"Reboot to firmware setup."},
#endif
    {"version",&print_version,"Print version."},
    {"copyright",&print_copyright,"Print copyright."},
    {"help",&console_help,"Print this help message."},
};

const struct Command editor_command = {"editor",&open_editor,"Open an empty boot entry editor."}; 
//separate from the array so its disabled when editor is disabled

const int size_of_commands = sizeof(commands) / sizeof(struct Command);

static void help_with_a_command(struct Command command) {
    print(command.name);
    for(size_t i = 0; i < 10 - strlen(command.name); i++) {
        print(" ");
    }
    print("-- ");
    print(command.description);
    print("\n");
}

static void console_help(void) {

    for(int i = 0; i < size_of_commands; i++) {
       help_with_a_command(commands[i]);
    }
    
    if (editor_enabled) {
        help_with_a_command(editor_command);
    }
}

const size_t PROMPT_SIZE = 256;
void console(void) {
    print("Welcome to the Limine console.\nType 'help' for more information.\n\n");

    char *prompt = ext_mem_alloc(PROMPT_SIZE);
    console_mode = true;
    while (console_mode) {
        print(">>> ");
        readline("", prompt, PROMPT_SIZE);
        bool sucessfull_command = false; 
        
        for(int i = 0; i < size_of_commands; i++) {
            if(strcmp(commands[i].name, prompt) == 0) {
                (*commands[i].run_command)();
                sucessfull_command = true;
                break;
            } 
        }

        if (editor_enabled && strcmp(editor_command.name, prompt) == 0) {
            (*editor_command.run_command)();
            sucessfull_command = true;
        }

        if (!sucessfull_command && *prompt != 0) {
            print("Invalid command: `%s`.\n", prompt);
        }
    }

    reset_term();
    pmm_free(prompt, PROMPT_SIZE);
}

