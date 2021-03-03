#include <stdint.h>
#include <stddef.h>
#include <lib/uri.h>
#include <lib/blib.h>
#include <lib/part.h>
#include <lib/libc.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <lib/print.h>
#include <pxe/tftp.h>
#include <tinf/tinf.h>

// A URI takes the form of: resource://root/path
// The following function splits up a URI into its componenets
bool uri_resolve(char *uri, char **resource, char **root, char **path) {
    size_t length = strlen(uri) + 1;
    char *buf = ext_mem_alloc(length);
    memcpy(buf, uri, length);
    uri = buf;

    *resource = *root = *path = NULL;

    // Get resource
    for (size_t i = 0; ; i++) {
        if (strlen(uri + i) < 3)
            return false;

        if (!memcmp(uri + i, "://", 3)) {
            *resource = uri;
            uri[i] = 0;
            uri += i + 3;
            break;
        }
    }

    // Get root
    for (size_t i = 0; ; i++) {
        if (uri[i] == 0)
            return false;

        if (uri[i] == '/') {
            *root = uri;
            uri[i] = 0;
            uri += i + 1;
            break;
        }
    }

    // Get path
    if (*uri == 0)
        return false;
    *path = uri;

    return true;
}

// BIOS partitions are specified in the <BIOS drive>:<partition> form.
// The drive may be omitted, the partition cannot.
static bool parse_bios_partition(char *loc, uint8_t *drive, uint8_t *partition) {
    uint64_t val;

    for (size_t i = 0; ; i++) {
        if (loc[i] == 0)
            return false;

        if (loc[i] == ':') {
            loc[i] = 0;
            if (*loc == 0) {
                *drive = boot_drive;
            } else {
                val = strtoui(loc, NULL, 10);
                if (val < 1 || val > 16) {
                    panic("BIOS drive number outside range 1-16");
                }
                *drive = (val - 1) + 0x80;
            }
            loc += i + 1;
            break;
        }
    }

    if (*loc == 0)
        return false;

    val = strtoui(loc, NULL, 10);
    if (val < 1 || val > 256) {
        panic("BIOS partition number outside range 1-256");
    }
    *partition = val - 1;

    return true;
}

static bool uri_bios_dispatch(struct file_handle *fd, char *loc, char *path) {
    uint8_t drive, partition;

    struct volume volume = {0};
    if (!parse_bios_partition(loc, &drive, &partition))
        return false;

    if (!volume_get_by_coord(&volume, drive, partition))
        return false;

    if (fopen(fd, &volume, path))
        return false;

    return true;
}

static bool uri_guid_dispatch(struct file_handle *fd, char *guid_str, char *path) {
    struct guid guid;
    if (!string_to_guid_be(&guid, guid_str))
        return false;

    struct volume part = {0};
    if (!volume_get_by_guid(&part, &guid)) {
        if (!string_to_guid_mixed(&guid, guid_str))
            return false;

        if (!volume_get_by_guid(&part, &guid))
            return false;
    }

    if (fopen(fd, &part, path))
        return false;

    return true;
}

static bool uri_tftp_dispatch(struct file_handle *fd, char *root, char *path) {
    uint32_t ip;
    if (!strcmp(root, "")) {
        ip = 0;
    } else {
        if (inet_pton(root, &ip)) {
            panic("invalid ipv4 address: %s", root);
        }
        print("\nip: %x\n", ip);
    }

    struct tftp_file_handle *cfg = conv_mem_alloc(sizeof(struct tftp_file_handle));
    if(tftp_open(cfg, ip, 69, path)) {
        return false;
    }

    fd->is_memfile = false;
    fd->fd = cfg;
    fd->read = tftp_read;
    fd->size = cfg->file_size;
    return true;
}

static bool uri_boot_dispatch(struct file_handle *fd, char *s_part, char *path) {
    if (booted_from_pxe)
        return uri_tftp_dispatch(fd, s_part, path);

    int partition;

    if (s_part[0] != '\0') {
        uint64_t val = strtoui(s_part, NULL, 10);
        if (val < 1 || val > 256) {
            panic("Partition number outside range 1-256");
        }
        partition = val - 1;
    } else if (booted_from_cd || boot_partition != -1) {
        partition = boot_partition;
    } else {
        panic("Boot partition information is unavailable.");
    }

    struct volume part = {0};
    if (!volume_get_by_coord(&part, boot_drive, partition))
        return false;

    if (fopen(fd, &part, path))
        return false;

    return true;
}

bool uri_open(struct file_handle *fd, char *uri) {
    bool ret;

    char *resource, *root, *path;
    uri_resolve(uri, &resource, &root, &path);

    if (resource == NULL) {
        panic("No resource specified for URI `%s`.", uri);
    }

    bool compressed = false;
    if (*resource == '$') {
        compressed = true;
        resource++;
    }

    if (!strcmp(resource, "bios")) {
        ret = uri_bios_dispatch(fd, root, path);
    } else if (!strcmp(resource, "boot")) {
        ret = uri_boot_dispatch(fd, root, path);
    } else if (!strcmp(resource, "guid")) {
        ret = uri_guid_dispatch(fd, root, path);
    } else if (!strcmp(resource, "uuid")) {
        ret = uri_guid_dispatch(fd, root, path);
    } else if (!strcmp(resource, "tftp")) {
        ret = uri_tftp_dispatch(fd, root, path);
    } else {
        panic("Resource `%s` not valid.", resource);
    }

    if (compressed && ret) {
        struct file_handle compressed_fd = {0};
        fread(fd, &compressed_fd.size, fd->size - 4, sizeof(uint32_t));
        compressed_fd.fd = ext_mem_alloc_aligned(compressed_fd.size, 4096);
        void *src = ext_mem_alloc(fd->size);
        fread(fd, src, 0, fd->size);
        if (tinf_gzip_uncompress(compressed_fd.fd, src, fd->size))
            panic("tinf error");
        compressed_fd.is_memfile = true;
        *fd = compressed_fd;
    }

    return ret;
}
