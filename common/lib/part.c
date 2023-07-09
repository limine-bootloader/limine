#include <stddef.h>
#include <lib/part.h>
#include <lib/print.h>
#include <lib/readline.h>

void list_volumes(void) {
    for (size_t i = 0; i < volume_index_i; i++) {
        struct volume *v = volume_index[i];
        print("index: %u\n", v->index);
        print("is_optical: %u\n", v->is_optical);
        print("partition: %u\n", v->partition);
        print("fslabel: %s\n", v->fslabel);
        print("sector_size: %u\n", v->sector_size);
        print("max_partition: %d\n", v->max_partition);
        print("first_sect: %U\n", v->first_sect);
        print("sect_count: %U\n", v->sect_count);
        if (i < volume_index_i - 1) {
            print("--- Press a key to continue ---\n");
            getchar();
        }
    }
}
