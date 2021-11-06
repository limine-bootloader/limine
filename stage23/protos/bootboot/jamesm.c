#include <protos/bootboot/initrd.h>
#include <lib/libc.h>
#include <stdint.h>

// This looks really sketchy, but that's what the official """spec""" says...
typedef struct initrd_header {
   unsigned char magic; // The magic number is there to check for consistency.
   char name[64];
   unsigned int offset; // Offset in the initrd the file starts.
   unsigned int length; // Length of the file.
} initrd_entry_t;

INITRD_HANDLER(jamesm) {
    if (file.size < 5) return (file_t){};

    uint32_t file_count = *((uint32_t*)(file.data));
    uint32_t path_len = strlen(path);

    initrd_entry_t* data = (initrd_entry_t*)(file.data + 4);
    if (data[0].magic != 0xBF) return (file_t){};

    for(uint32_t i = 0;i < file_count;i++) {
        if (data[i].magic != 0xBF) return (file_t){};
        if(!memcmp(data[i].name, path, path_len + 1)){
            return (file_t){
                data[i].length,
                file.data + data[i].offset
            };
        }
    }
    return (file_t){};
}