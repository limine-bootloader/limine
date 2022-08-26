#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/guid.h>
#include <lib/misc.h>

bool is_valid_guid(const char *s) {
    for (size_t i = 0; ; i++) {
        switch (i) {
            case 8:
            case 13:
            case 18:
            case 23:
                if (s[i] != '-')
                    return false;
                break;
            case 36:
                return s[i] == 0;
            default:
                if (digit_to_int(s[i]) == -1)
                    return false;
                break;
        }
    }
}

static void guid_convert_le_cluster(uint8_t *dest, const char *s, int len) {
    size_t p = 0;
    for (int i = len - 1; i >= 0; i--) {
        int val = digit_to_int(s[i]);

        i % 2 ? (dest[p] = val) : (dest[p++] |= val << 4);
    }
}

static void guid_convert_be_cluster(uint8_t *dest, const char *s, int len) {
    size_t p = 0;
    for (int i = 0; i < len; i++) {
        int val = digit_to_int(s[i]);

        i % 2 ? (dest[p++] |= val) : (dest[p] = val << 4);
    }
}

bool string_to_guid_be(struct guid *guid, const char *s) {
    if (!is_valid_guid(s))
        return false;

    guid_convert_be_cluster((uint8_t *)guid + 0,  s + 0,  8);
    guid_convert_be_cluster((uint8_t *)guid + 4,  s + 9,  4);
    guid_convert_be_cluster((uint8_t *)guid + 6,  s + 14, 4);
    guid_convert_be_cluster((uint8_t *)guid + 8,  s + 19, 4);
    guid_convert_be_cluster((uint8_t *)guid + 10, s + 24, 12);

    return true;
}

bool string_to_guid_mixed(struct guid *guid, const char *s) {
    if (!is_valid_guid(s))
        return false;

    guid_convert_le_cluster((uint8_t *)guid + 0,  s + 0,  8);
    guid_convert_le_cluster((uint8_t *)guid + 4,  s + 9,  4);
    guid_convert_le_cluster((uint8_t *)guid + 6,  s + 14, 4);
    guid_convert_be_cluster((uint8_t *)guid + 8,  s + 19, 4);
    guid_convert_be_cluster((uint8_t *)guid + 10, s + 24, 12);

    return true;
}
