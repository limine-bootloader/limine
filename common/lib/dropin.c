#if defined (UEFI)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <efi.h>
#include <lib/dropin.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/print.h>

#define DROPIN_DIR L"\\EFI\\systemd\\drivers"

// Nothing legitimate comes close; the bound is only there to stop a walk of
// a device path the firmware never terminated.
#define DROPIN_MAX_DEVICE_PATH 4096

// The interface names the drivers a firmware can run for this machine by the
// suffix of their filename, spelt as the UEFI removable media path does.
#if defined (__x86_64__)
#  define DROPIN_SUFFIX "x64.efi"
#elif defined (__i386__)
#  define DROPIN_SUFFIX "ia32.efi"
#elif defined (__aarch64__)
#  define DROPIN_SUFFIX "aa64.efi"
#elif defined (__riscv)
#  define DROPIN_SUFFIX "riscv64.efi"
#elif defined (__loongarch64)
#  define DROPIN_SUFFIX "loongarch64.efi"
#else
#error "Unspecified architecture"
#endif

static size_t wstrlen(const CHAR16 *s) {
    size_t len = 0;
    while (s[len] != 0) {
        len++;
    }
    return len;
}

static bool name_is_for_this_machine(const CHAR16 *name) {
    static const char suffix[] = DROPIN_SUFFIX;
    size_t suffix_len = SIZEOF_ARRAY(suffix) - 1;

    size_t name_len = wstrlen(name);
    if (name_len < suffix_len) {
        return false;
    }

    const CHAR16 *tail = name + (name_len - suffix_len);
    for (size_t i = 0; i < suffix_len; i++) {
        CHAR16 c = tail[i];
        if (c >= L'A' && c <= L'Z') {
            c += L'a' - L'A';
        }
        if (c != (CHAR16)suffix[i]) {
            return false;
        }
    }

    return true;
}

// LoadImage() is given a path rather than a buffer so that the firmware
// verifies the driver against its own Secure Boot policy.
static EFI_DEVICE_PATH_PROTOCOL *build_driver_device_path(EFI_HANDLE device_handle,
                                                          const CHAR16 *name) {
    EFI_GUID device_path_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_DEVICE_PATH_PROTOCOL *volume_path = NULL;

    if (gBS->HandleProtocol(device_handle, &device_path_guid,
                            (void **)&volume_path) != EFI_SUCCESS) {
        return NULL;
    }

    size_t prefix_len = 0;
    for (EFI_DEVICE_PATH_PROTOCOL *node = volume_path; !IsDevicePathEnd(node);
         node = NextDevicePathNode(node)) {
        size_t node_len = DevicePathNodeLength(node);
        if (node_len < sizeof(EFI_DEVICE_PATH_PROTOCOL)
         || prefix_len > DROPIN_MAX_DEVICE_PATH) {
            return NULL;
        }
        prefix_len += node_len;
    }

    size_t prefix_chars = SIZEOF_ARRAY(DROPIN_DIR L"\\") - 1;
    size_t name_chars = wstrlen(name);

    size_t path_item_len = sizeof(EFI_DEVICE_PATH_PROTOCOL)
                         + (prefix_chars + name_chars + 1) * sizeof(CHAR16);
    if (path_item_len > 0xffff) {
        return NULL;
    }

    size_t alloc_len = prefix_len + path_item_len + END_DEVICE_PATH_LENGTH;

    EFI_DEVICE_PATH_PROTOCOL *device_path = NULL;
    if (gBS->AllocatePool(EfiLoaderData, alloc_len,
                          (void **)&device_path) != EFI_SUCCESS) {
        return NULL;
    }

    memcpy(device_path, volume_path, prefix_len);

    FILEPATH_DEVICE_PATH *path_item = (void *)device_path + prefix_len;
    path_item->Header.Type      = MEDIA_DEVICE_PATH;
    path_item->Header.SubType   = MEDIA_FILEPATH_DP;
    path_item->Header.Length[0] = path_item_len;
    path_item->Header.Length[1] = path_item_len >> 8;

    CHAR16 *path_name = path_item->PathName;
    memcpy(path_name, DROPIN_DIR L"\\", prefix_chars * sizeof(CHAR16));
    memcpy(&path_name[prefix_chars], name, (name_chars + 1) * sizeof(CHAR16));

    EFI_DEVICE_PATH_PROTOCOL *end_item = (void *)path_item + path_item_len;
    end_item->Type      = END_DEVICE_PATH_TYPE;
    end_item->SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    end_item->Length[0] = END_DEVICE_PATH_LENGTH;
    end_item->Length[1] = END_DEVICE_PATH_LENGTH >> 8;

    return device_path;
}

// Opening a plain file as though it were a directory succeeds, and reading
// it would hand us its contents where the entry names should be.
static bool file_is_directory(EFI_FILE_HANDLE file) {
    EFI_GUID info_guid = EFI_FILE_INFO_ID;
    uint64_t buf[64];
    UINTN size = sizeof(buf);

    if (file->GetInfo(file, &info_guid, &size, buf) != EFI_SUCCESS) {
        return false;
    }

    return (((EFI_FILE_INFO *)buf)->Attribute & EFI_FILE_DIRECTORY) != 0;
}

static bool load_one_driver(EFI_HANDLE parent_image, EFI_HANDLE device_handle,
                            const CHAR16 *name) {
    EFI_DEVICE_PATH_PROTOCOL *device_path = build_driver_device_path(device_handle, name);
    if (device_path == NULL) {
        return false;
    }

    EFI_HANDLE image = NULL;
    EFI_STATUS status = gBS->LoadImage(false, parent_image, device_path, NULL, 0, &image);

    gBS->FreePool(device_path);

    if (status != EFI_SUCCESS) {
        printv("dropin: LoadImage() failure (%X)\n", (uint64_t)status);
        return false;
    }

    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    status = gBS->HandleProtocol(image, &loaded_img_prot_guid, (void **)&loaded_image);

    // Anything that is not driver code would be started as an application.
    if (status != EFI_SUCCESS
     || (loaded_image->ImageCodeType != EfiBootServicesCode
      && loaded_image->ImageCodeType != EfiRuntimeServicesCode)) {
        printv("dropin: Image is not a driver, refusing it\n");
        gBS->UnloadImage(image);
        return false;
    }

    status = gBS->StartImage(image, NULL, NULL);
    if (status != EFI_SUCCESS) {
        // A driver that only initialises says so by returning EFI_ABORTED
        // on success, so that it is unloaded again afterwards.
        if (status != EFI_ABORTED) {
            printv("dropin: StartImage() failure (%X)\n", (uint64_t)status);
        }
        gBS->UnloadImage(image);
        return false;
    }

    return true;
}

static void reconnect_all_controllers(void) {
    UINTN handle_count = 0;
    EFI_HANDLE *handles = NULL;

    if (gBS->LocateHandleBuffer(AllHandles, NULL, NULL,
                                &handle_count, &handles) != EFI_SUCCESS) {
        return;
    }

    // Handles go stale as their controllers are reconnected, and firmware
    // hands out bogus ones besides, so failures here are expected.
    for (UINTN i = 0; i < handle_count; i++) {
        gBS->ConnectController(handles[i], NULL, NULL, true);
    }

    gBS->FreePool(handles);
}

void dropin_load_drivers(EFI_HANDLE parent_image, EFI_HANDLE device_handle) {
    if (device_handle == NULL) {
        return;
    }

    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    if (gBS->HandleProtocol(device_handle, &fs_guid, (void **)&fs) != EFI_SUCCESS) {
        return;
    }

    EFI_FILE_HANDLE root = NULL;
    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS) {
        return;
    }

    EFI_FILE_HANDLE dir = NULL;
    EFI_STATUS status = root->Open(root, &dir, DROPIN_DIR, EFI_FILE_MODE_READ, 0);

    root->Close(root);

    if (status != EFI_SUCCESS) {
        return;
    }

    if (!file_is_directory(dir)) {
        dir->Close(dir);
        return;
    }

    size_t info_size = SIZE_OF_EFI_FILE_INFO + 256 * sizeof(CHAR16);
    EFI_FILE_INFO *info = NULL;
    if (gBS->AllocatePool(EfiLoaderData, info_size, (void **)&info) != EFI_SUCCESS) {
        dir->Close(dir);
        return;
    }

    size_t loaded = 0;
    for (;;) {
        UINTN read_size = info_size;
        status = dir->Read(dir, &read_size, info);

        if (status == EFI_BUFFER_TOO_SMALL) {
            EFI_FILE_INFO *bigger = NULL;
            if (gBS->AllocatePool(EfiLoaderData, read_size, (void **)&bigger) != EFI_SUCCESS) {
                break;
            }
            gBS->FreePool(info);
            info = bigger;
            info_size = read_size;
            continue;
        }

        // A zero-length read is the end of the directory.
        if (status != EFI_SUCCESS || read_size == 0) {
            break;
        }

        if (info->FileName[0] == L'.'
         || (info->Attribute & EFI_FILE_DIRECTORY) != 0
         || !name_is_for_this_machine(info->FileName)) {
            continue;
        }

        if (load_one_driver(parent_image, device_handle, info->FileName)) {
            loaded++;
        }
    }

    gBS->FreePool(info);
    dir->Close(dir);

    if (loaded > 0) {
        printv("dropin: Loaded %u drop-in driver(s)\n", (unsigned)loaded);
        reconnect_all_controllers();
    }
}

#endif
