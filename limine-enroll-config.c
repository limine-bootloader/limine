#undef IS_WINDOWS
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define IS_WINDOWS 1
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_B2SUM_SIGNATURE "++CONFIG_B2SUM_SIGNATURE++"

static void usage(const char *name) {
    printf("Usage: %s <Limine executable> <BLAKE2B of config file>\n", name);
    printf("\n");
    printf("    --reset      Remove enrolled BLAKE2B, will not check config integrity\n");
    printf("\n");
    printf("    --quiet      Do not print verbose diagnostic messages\n");
    printf("\n");
    printf("    --help | -h  Display this help message\n");
    printf("\n");
}

static void remove_arg(int *argc, char *argv[], int index) {
    for (int i = index; i < *argc - 1; i++) {
        argv[i] = argv[i + 1];
    }

    (*argc)--;

    argv[*argc] = NULL;
}

int main(int argc, char *argv[]) {
    int ret = EXIT_FAILURE;

    char *bootloader = NULL;
    FILE *bootloader_file = NULL;
    bool quiet = false;
    bool reset = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            remove_arg(&argc, argv, i);
            quiet = true;
        } else if (strcmp(argv[i], "--reset") == 0) {
            remove_arg(&argc, argv, i);
            reset = true;
        }
    }

    if (argc <= (reset ? 1 : 2)) {
        usage(argv[0]);
#ifdef IS_WINDOWS
        system("pause");
#endif
        return EXIT_FAILURE;
    }

    if (!reset && strlen(argv[2]) != 128) {
        fprintf(stderr, "ERROR: BLAKE2B specified is not 128 characters long\n");
        goto cleanup;
    }

    bootloader_file = fopen(argv[1], "r+b");
    if (bootloader_file == NULL) {
        perror("ERROR");
        goto cleanup;;
    }

    if (fseek(bootloader_file, 0, SEEK_END) != 0) {
        perror("ERROR");
        goto cleanup;
    }
    size_t bootloader_size = ftell(bootloader_file);
    rewind(bootloader_file);

    bootloader = malloc(bootloader_size);
    if (bootloader == NULL) {
        perror("ERROR");
        goto cleanup;
    }

    if (fread(bootloader, bootloader_size, 1, bootloader_file) != 1) {
        perror("ERROR");
        goto cleanup;
    }

    char *checksum_loc = NULL;
    size_t checked_count = 0;
    const char *config_b2sum_sign = CONFIG_B2SUM_SIGNATURE;
    for (size_t i = 0; i < bootloader_size - ((sizeof(CONFIG_B2SUM_SIGNATURE) - 1) + 128) + 1; i++) {
        if (bootloader[i] != config_b2sum_sign[checked_count]) {
            checked_count = 0;
            continue;
        }

        checked_count++;

        if (checked_count == sizeof(CONFIG_B2SUM_SIGNATURE) - 1) {
            checksum_loc = &bootloader[i + 1];
            break;
        }
    }

    if (checksum_loc == NULL) {
        fprintf(stderr, "ERROR: Checksum location not found in provided executable\n");
        goto cleanup;
    }

    if (!reset) {
        memcpy(checksum_loc, argv[2], 128);
    } else {
        memset(checksum_loc, '0', 128);
    }

    if (fseek(bootloader_file, 0, SEEK_SET) != 0) {
        perror("ERROR");
        goto cleanup;
    }
    if (fwrite(bootloader, bootloader_size, 1, bootloader_file) != 1) {
        perror("ERROR");
        goto cleanup;
    }

    if (!quiet) {
        fprintf(stderr, "Config file BLAKE2B successfully %s!\n", reset ? "reset" : "enrolled");
    }
    ret = EXIT_SUCCESS;

cleanup:
    if (bootloader != NULL) {
        free(bootloader);
    }
    if (bootloader_file != NULL) {
        fclose(bootloader_file);
    }
    return ret;
}
