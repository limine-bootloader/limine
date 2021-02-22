#ifndef __LIB__GUID_H__
#define __LIB__GUID_H__

#include <stdint.h>
#include <stdbool.h>

struct guid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t  d[8];
} __attribute__((packed));

bool is_valid_guid(const char *s);
bool string_to_guid_be(struct guid *guid, const char *s);
bool string_to_guid_mixed(struct guid *guid, const char *s);

#endif
