#ifndef __ARCH__AARCH64__DTB_H__
#define __ARCH__AARCH64__DTB_H__

#include <lib/blib.h>

struct dtb {
    void* dtb;
};
struct dtb_ref {
    struct dtb* dtb;
    size_t offset;
};

struct dtb dtb_open(void* dtb);
size_t dtb_sizeof(void* dtb);

struct dtb_ref dtb_get_root(struct dtb* dtb);
bool dtb_get_node(struct dtb_ref* ref, struct dtb_ref* new, const char* id);
bool dtb_get_prop(struct dtb_ref* ref, const char* id, uint32_t* size, void** data);

bool dtb_next_node(struct dtb_ref* ref, const char** id, struct dtb_ref* node);
bool dtb_next_prop(struct dtb_ref* ref, const char** id, uint32_t* size, void** data);

bool dtb_next_node_nested(struct dtb_ref* ref, const char** id, struct dtb_ref* node);
bool dtb_init(struct dtb* dtb);

void dtb_dump_node(struct dtb_ref r, int depth);

#endif