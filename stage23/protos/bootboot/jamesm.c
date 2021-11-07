#include <stdint.h>
#include <stddef.h>
#include <protos/bootboot/initrd.h>
#include <lib/libc.h>

// This looks really sketchy, but that's what the official """spec""" says...
struct initrd_entry {
   uint8_t magic; // The magic number is there to check for consistency.
   char name[64];
   uint32_t offset; // Offset in the initrd the file starts.
   uint32_t length; // Length of the file.
};

INITRD_HANDLER(jamesm) {
    if (file.size < 5) {
        return (struct initrd_file){0};
    }

    uint32_t file_count = *((uint32_t *)(file.data));
    size_t path_len = strlen(path);

    struct initrd_entry *data = (void *)(file.data + 4);
    if (data->magic != 0xbf) {
        return (struct initrd_file){0};
    }

    for (uint32_t i = 0; i < file_count; i++) {
        if (data[i].magic != 0xbf) {
            return (struct initrd_file){0};
        }

        if (!memcmp(data[i].name, path, path_len + 1)) {
            return (struct initrd_file){
                data[i].length,
                file.data + data[i].offset
            };
        }
    }

    return (struct initrd_file){0};
}
