#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <path to qloader2 binary> <device> [qloader2 start sector]\n", argv[0]);
        return 1;
    }

    FILE *ql2_bin = fopen(argv[1], "rb");
    if (ql2_bin == NULL) {
        perror("Error: ");
        return 1;
    }

    FILE *device = fopen(argv[2], "r+b");
    if (device == NULL) {
        perror("Error: ");
        fclose(ql2_bin);
        return 1;
    }

    uint32_t stage2_sect = 1;
    if (argc >= 4)
        sscanf(argv[3], "%" SCNu32, &stage2_sect);

    char orig_mbr[64];
    fseek(device, 446, SEEK_SET);
    fread(orig_mbr, 1, 64, device);

    char ql2_bootsect[512];
    fseek(ql2_bin, 0, SEEK_SET);
    fread(ql2_bootsect, 1, 512, ql2_bin);
    fseek(device, 0, SEEK_SET);
    fwrite(ql2_bootsect, 1, 512, device);

    char *ql2_stage2 = malloc(63 * 512);
    fseek(ql2_bin, 512, SEEK_SET);
    fread(ql2_stage2, 63, 512, ql2_bin);
    fseek(device, stage2_sect * 512, SEEK_SET);
    fwrite(ql2_stage2, 63, 512, device);
    free(ql2_stage2);

    fseek(device, 0x1b0, SEEK_SET);
    fwrite(&stage2_sect, 1, 4, device);

    fseek(device, 446, SEEK_SET);
    fwrite(orig_mbr, 1, 64, device);

    fclose(ql2_bin);
    fclose(device);

    return 0;
}
