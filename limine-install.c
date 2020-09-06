#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

extern char _binary_src_limine_bin_start[];

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <device> [stage2 start sector]\n", argv[0]);
        return 1;
    }

    FILE *device = fopen(argv[1], "r+b");
    if (device == NULL) {
        perror("Error: ");
        return 1;
    }

    uint32_t stage2_sect = 1;
    if (argc >= 3)
        sscanf(argv[2], "%" SCNu32, &stage2_sect);

    // Save original timestamp
    uint8_t timestamp[6];
    fseek(device, 218, SEEK_SET);
    fread(timestamp, 1, 6, device);

    // Save the original partition table of the device
    uint8_t orig_mbr[70];
    fseek(device, 440, SEEK_SET);
    fread(orig_mbr, 1, 70, device);

    // Write the bootsector from the bootloader to the device
    fseek(device, 0, SEEK_SET);
    fwrite(&_binary_src_limine_bin_start[0], 1, 512, device);

    // Write the rest of stage 2 to the device
    fseek(device, stage2_sect * 512, SEEK_SET);
    fwrite(&_binary_src_limine_bin_start[512], 63, 512, device);

    // Hardcode in the bootsector the location of stage 2
    fseek(device, 0x1b0, SEEK_SET);
    fwrite(&stage2_sect, 1, sizeof(uint32_t), device);

    // Write back timestamp
    fseek(device, 218, SEEK_SET);
    fwrite(timestamp, 1, 6, device);

    // Write back the saved partition table to the device
    fseek(device, 440, SEEK_SET);
    fwrite(orig_mbr, 1, 70, device);

    fclose(device);

    return 0;
}
