#include <lib/blib.h>
#include <lib/print.h>
#include <arch/aarch64/dtb.h>

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

void dtb_dump_node(struct dtb_ref r, int depth) {
    {
        struct dtb_ref r2 = r;

        const char* id = "<NO WRITE>";
        uint32_t size;
        void* data;
        while (dtb_next_prop(&r2, &id, &size, &data)) {
            for (int i = 0;i < depth;i++) print(" ");
            bool is_valid_str = size ? true : false;
            bool use_stringset = false;
            bool hasnonnul = false;
            if (!strcmp(id, "reg")) is_valid_str = false;
            for (uint32_t i = 0;i < size;i++) {
                uint8_t byte = *(uint8_t*)(data + i);
                if (i == size - 1) {
                    if (byte) is_valid_str = false;
                } else {
                    if (byte) {
                        if (byte < 0x20 || byte > 0x7f) is_valid_str = false;
                        hasnonnul = true;
                    } else {
                        use_stringset = true;
                    }
                }
            }
            if (is_valid_str && hasnonnul) {
                if (use_stringset) {
                    print("%s = {", id);
                    for (uint32_t i = 0;i < size;i += strlen(data + i) + 1) {
                        if (i) print(" ");
                        print("\"%s\"", data + i);
                    }
                    print("}\n");
                } else {
                    print("%s = \"%s\"\n", id, data);
                }
            } else {
                if (!(size % 4) && size) {
                    print("%s = <", id);
                    for (uint32_t i = 0;i < size;i += 4) {
                        if (i) print(" ");
                        print("%x", bswap32(*(uint32_t*)(data + i)));
                    }
                    print(">\n");
                } else {
                    print("%s = [", id);
                    for (uint32_t i = 0;i < size;i++) {
                        if (i) print(" ");
                        print("%x", (uint32_t)*(uint8_t*)(data + i));
                    }
                    print("]\n");
                }
            }
        }
    }
    {
        struct dtb_ref r2 = r;

        const char* id = "<NO WRITE>";
        struct dtb_ref ref2;
        while (dtb_next_node(&r2, &id, &ref2)) {
            for (int i = 0;i < depth;i++) print(" ");
            print("%s {\n", id);
            dtb_dump_node(ref2, depth+4);
            for (int i = 0;i < depth;i++) print(" ");
            print("}\n");
        }
    }
}
struct dtb dtb_open(void *dtb) {
    if (((struct fdt_header*)dtb)->magic != bswap32(0xd00dfeed)) panic(false, "Invalid DTB opened!");
    return (struct dtb){dtb};
}
struct dtb_ref dtb_get_root(struct dtb* dtb) {
    return (struct dtb_ref){.dtb=dtb,.offset=bswap32(((struct fdt_header*)dtb->dtb)->off_dt_struct)};
}
void dtb_skip_opcode(struct dtb_ref* ref) {
    uint32_t typ = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset));
    ref->offset += 4;
    if (typ == /* begin node */ 1) {
        while (*(uint8_t*)(ref->dtb->dtb + ref->offset)) ref->offset++;
        ref->offset++;
        while (ref->offset & 3) ref->offset++;
        while (true) {
            uint32_t subtype = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset));
            if (subtype == /* end node */ 2) break;
            dtb_skip_opcode(ref);
        }
        ref->offset += 4;
        return;
    }
    if (typ == /* prop */ 3) {
        uint32_t len = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset));
        ref->offset += 8;
        ref->offset += len;
        while (ref->offset & 3) ref->offset++;
        return;
    }
    if (typ == 4) return;
    if (typ == 9) return;
    panic(false, "TODO: dtb opcode %x", typ);
}
bool dtb_next_prop(struct dtb_ref* ref, const char** id, uint32_t* size, void** data) {
    while (true) {
        uint32_t typ = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset));
        if (typ == /* end node */ 2) return false; // no more data!
        if (typ == /* end of data */ 9) return false; // no more data either!
        if (typ != /* begin prop */ 3) {
            dtb_skip_opcode(ref);
            continue;
        }
        uint32_t len = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset + 4));
        uint32_t nameoff = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset + 8));
        ref->offset += 12;
        *id = (const char*)(ref->dtb->dtb + bswap32(((struct fdt_header*)ref->dtb->dtb)->off_dt_strings) + nameoff);
        *size = len;
        *data = ref->dtb->dtb + ref->offset;
        ref->offset += len;
        while (ref->offset & 3) ref->offset++;
        return true;
    }
}
bool dtb_next_node(struct dtb_ref* ref, const char** name, struct dtb_ref* cr) {
    while (true) {
        uint32_t typ = bswap32(*(uint32_t*)(ref->dtb->dtb + ref->offset));
        if (typ == /* end node */ 2) return false; // no more nodes!
        if (typ == /* end of data */ 9) return false; // no more data!
        if (typ != /* begin node */ 1) {
            dtb_skip_opcode(ref);
            continue;
        }
        *name = ref->dtb->dtb + ref->offset + 4;
        cr->dtb = ref->dtb;
        cr->offset = ref->offset + 4;
        while (*(uint8_t*)(cr->dtb->dtb + cr->offset)) cr->offset++;
        cr->offset++;
        while (cr->offset & 3) cr->offset++;
        dtb_skip_opcode(ref);
        return true;
    }
}

static void* cached_dtb = NULL;
static bool is_dtb_ready = false;

bool dtb_init(struct dtb* dtb) {
    if (is_dtb_ready) {
        dtb->dtb = cached_dtb;
        return cached_dtb ? true : false;
    }
    is_dtb_ready = true;
    for (uint64_t i = 0;i < gST->NumberOfTableEntries;i++) {
        if (!memcmp(&gST->ConfigurationTable[i].VendorGuid, &(EFI_GUID)EFI_DTB_TABLE_GUID, sizeof(EFI_GUID))) {
            cached_dtb = gST->ConfigurationTable[i].VendorTable;
        }
    }
    dtb->dtb = cached_dtb;
    return cached_dtb ? true : false;
}
bool dtb_get_prop(struct dtb_ref* ref, const char* id, uint32_t* size, void** data) {
    struct dtb_ref refclone = *ref;
    const char* cmpid;
    while (dtb_next_prop(&refclone, &cmpid, size, data)) {
        if (!strcmp(cmpid, id)) return true;
    }
    return false;
}
