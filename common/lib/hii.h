#ifndef LIB__HII_H__
#define LIB__HII_H__

#if defined (UEFI)

#include <stddef.h>
#include <stdbool.h>

// Writes the RFC 4646 language tag of the keyboard layout the firmware has
// active (e.g. `en-US`) into `buf`, NUL terminated. Returns false where the
// firmware provides no HII database, or its layout carries no tag.
bool hii_get_keyboard_layout(wchar_t *buf, size_t buf_size);

#endif

#endif
