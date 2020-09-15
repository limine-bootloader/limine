#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

int main(int argc, char *argv[]) {
    FILE    *bootloader_file, *device;
    uint8_t *bootloader_img;
    uint8_t  orig_mbr[70], timestamp[6];
    uint32_t stage2_sect;

    if (argc < 3) {
        printf("Usage: %s <bootloader image> <device> [stage2 start sector]\n", argv[0]);
        return 1;
    }

    bootloader_file = fopen(argv[1], "rb");
    if (bootloader_file == NULL) {
        perror("Error: ");
        return 1;
    }

    // The bootloader image is 64 sectors (32k)
    bootloader_img = malloc(64 * 512);
    if (bootloader_img == NULL) {
        perror("Error: ");
        fclose(bootloader_file);
        return 1;
    }

    // Load in bootloader image
    fseek(bootloader_file, 0, SEEK_SET);
    fread(bootloader_img, 64, 512, bootloader_file);
    fclose(bootloader_file);

    device = fopen(argv[2], "r+b");
    if (device == NULL) {
        perror("Error: ");
        free(bootloader_img);
        return 1;
    }

    stage2_sect = 1;
    if (argc >= 4)
        sscanf(argv[3], "%" SCNu32, &stage2_sect);

    // Save original timestamp
    fseek(device, 218, SEEK_SET);
    fread(timestamp, 1, 6, device);

    // Save the original partition table of the device
    fseek(device, 440, SEEK_SET);
    fread(orig_mbr, 1, 70, device);

    // Write the bootsector from the bootloader to the device
    fseek(device, 0, SEEK_SET);
    fwrite(&bootloader_img[0], 1, 512, device);

    // Write the rest of stage 2 to the device
    fseek(device, stage2_sect * 512, SEEK_SET);
    fwrite(&bootloader_img[512], 63, 512, device);

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
    free(bootloader_img);

    return 0;
}
