#include <stdio.h>

int main(void) {
    int ok = 0;

    FILE *limine_hdd = fopen("limine-hdd.bin", "r+b");
    if (limine_hdd == NULL) {
        goto err;
    }

    printf("const uint8_t _binary_limine_hdd_bin_data[] = {\n\t");

    int c = fgetc(limine_hdd);
    for (size_t i = 0; ; i++) {
        printf("0x%02x", c);

        c = fgetc(limine_hdd);
        if (c == EOF) {
            break;
        }

        printf(", ");
        if (i % 12 == 11) {
            printf("\n\t");
        }
    }

    printf("\n};\n");

    goto cleanup;

err:
    perror("ERROR");
    ok = 1;

cleanup:
    if (limine_hdd != NULL) {
        fclose(limine_hdd);
    }

    return ok;
}
