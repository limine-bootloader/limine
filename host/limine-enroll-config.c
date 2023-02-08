#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_B2SUM_SIGNATURE "++CONFIG_B2SUM_SIGNATURE++"

int main(int argc, char *argv[]) {
    int ret = 1;

    char *bootloader = NULL;
    FILE *bootloader_file = NULL;

    if (argc <= 2) {
        fprintf(stderr, "usage: %s <Limine bootloader executable> <128-byte BLAKE2B of config file>\n", argv[0]);
        goto cleanup;
    }

    if (strlen(argv[2]) != 128) {
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

    memcpy(checksum_loc, argv[2], 128);

    if (fseek(bootloader_file, 0, SEEK_SET) != 0) {
        perror("ERROR");
        goto cleanup;
    }
    if (fwrite(bootloader, bootloader_size, 1, bootloader_file) != 1) {
        perror("ERROR");
        goto cleanup;
    }

    fprintf(stderr, "Config file BLAKE2B successfully enrolled!\n");
    ret = 0;

cleanup:
    if (bootloader != NULL) {
        free(bootloader);
    }
    if (bootloader_file != NULL) {
        fclose(bootloader_file);
    }
    return ret;
}
