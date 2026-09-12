#if defined (UEFI)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <efi.h>
#include <lib/hii.h>
#include <lib/libc.h>
#include <lib/misc.h>

#define EFI_HII_DATABASE_PROTOCOL_GUID \
    { 0xef9fc172, 0xa1b2, 0x4693, { 0xb3, 0x27, 0x6d, 0x32, 0xfc, 0x41, 0x60, 0x42 } }

struct hii_database_protocol;

// Only GetKeyboardLayout() is ever called; the slots around it are here to
// give it the right offset into the protocol's function table.
struct hii_database_protocol {
    void *new_package_list;
    void *remove_package_list;
    void *update_package_list;
    void *list_package_lists;
    void *export_package_lists;
    void *register_package_notify;
    void *unregister_package_notify;
    void *find_keyboard_layouts;
    EFI_STATUS (EFIAPI *get_keyboard_layout)(struct hii_database_protocol *self,
                                             EFI_GUID *key_guid,
                                             UINT16 *keyboard_layout_length,
                                             void *keyboard_layout);
    void *set_keyboard_layout;
    void *get_package_list_handle;
};

// EFI_HII_KEYBOARD_LAYOUT. The GUID starts at offset 2, so nothing that
// follows it lands on its natural alignment.
struct hii_keyboard_layout {
    uint16_t layout_length;
    EFI_GUID guid;
    uint32_t descriptor_string_offset;
    uint8_t descriptor_count;
} __attribute__((packed));

// The bundle descriptor_string_offset points at. Each of the
// description_count entries is a language tag, a U+0020, then a NUL
// terminated description; the tag itself is only delimited by that space.
struct hii_description_string_bundle {
    uint16_t description_count;
    wchar_t strings[];
} __attribute__((packed));

bool hii_get_keyboard_layout(wchar_t *buf, size_t buf_size) {
    EFI_GUID hii_db_guid = EFI_HII_DATABASE_PROTOCOL_GUID;
    struct hii_database_protocol *hii_db = NULL;

    if (gBS->LocateProtocol(&hii_db_guid, NULL, (void **)&hii_db) != EFI_SUCCESS
     || hii_db == NULL) {
        return false;
    }

    // The sizing call is defined to fail; a success means the firmware did
    // not understand the request.
    UINT16 length = 0;
    if (hii_db->get_keyboard_layout(hii_db, NULL, &length, NULL) != EFI_BUFFER_TOO_SMALL
     || length < sizeof(struct hii_keyboard_layout)) {
        return false;
    }

    struct hii_keyboard_layout *layout = NULL;
    if (gBS->AllocatePool(EfiLoaderData, length, (void **)&layout) != EFI_SUCCESS) {
        return false;
    }

    bool ret = false;

    UINT16 got = length;
    if (hii_db->get_keyboard_layout(hii_db, NULL, &got, layout) != EFI_SUCCESS
     || got != length || layout->layout_length != length) {
        goto out;
    }

    uint32_t offset = layout->descriptor_string_offset;
    if (offset > length
     || length - offset < sizeof(struct hii_description_string_bundle)) {
        goto out;
    }

    struct hii_description_string_bundle *bundle = (void *)layout + offset;
    if (bundle->description_count == 0) {
        goto out;
    }

    size_t avail = (length - offset - sizeof(struct hii_description_string_bundle))
                 / sizeof(wchar_t);

    size_t len = 0;
    while (len < avail && bundle->strings[len] != L' ') {
        len++;
    }
    if (len == 0 || len == avail || len + 1 > buf_size) {
        goto out;
    }

    memcpy(buf, bundle->strings, len * sizeof(wchar_t));
    buf[len] = L'\0';
    ret = true;

out:
    gBS->FreePool(layout);
    return ret;
}

#endif
