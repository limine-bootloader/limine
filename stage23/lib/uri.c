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

static bool parse_bios_partition(char *loc, int *drive, int *partition) {
    int64_t val;

    for (size_t i = 0; ; i++) {
        if (loc[i] == 0)
            return false;

        if (loc[i] == ':') {
            loc[i] = 0;
            if (*loc == 0) {
                panic("Drive number cannot be omitted for hdd:// and odd://");
            } else {
                val = strtoui(loc, NULL, 10);
                if (val < 1 || val > 16) {
                    panic("Drive number outside range 1-16");
                }
                *drive = val;
            }
            loc += i + 1;
            break;
        }
    }

    if (*loc == 0) {
        *partition = -1;
        return true;
    }

    val = strtoui(loc, NULL, 10);
    if (val < 1 || val > 256) {
        panic("Partition number outside range 1-256");
    }
    *partition = val - 1;

    return true;
}

static bool uri_hdd_dispatch(struct file_handle *fd, char *loc, char *path) {
    int drive, partition;

    if (!parse_bios_partition(loc, &drive, &partition))
        return false;

    drive = (drive - 1) + 0x80;

    struct volume *volume = volume_get_by_coord(drive, partition);

    if (volume == NULL)
        return false;

    if (fopen(fd, volume, path))
        return false;

    return true;
}

static bool uri_odd_dispatch(struct file_handle *fd, char *loc, char *path) {
    int drive, partition;

    if (!parse_bios_partition(loc, &drive, &partition))
        return false;

    drive = (drive - 1) + 0xe0;

    struct volume *volume = volume_get_by_coord(drive, partition);

    if (volume == NULL)
        return false;

    if (fopen(fd, volume, path))
        return false;

    return true;
}

static bool uri_guid_dispatch(struct file_handle *fd, char *guid_str, char *path) {
    struct guid guid;
    if (!string_to_guid_be(&guid, guid_str))
        return false;

    struct volume *volume = volume_get_by_guid(&guid);
    if (volume == NULL) {
        if (!string_to_guid_mixed(&guid, guid_str))
            return false;

        volume = volume_get_by_guid(&guid);
        if (volume == NULL)
            return false;
    }

    if (fopen(fd, volume, path))
        return false;

    return true;
}

#if defined (bios)
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
#endif

static bool uri_boot_dispatch(struct file_handle *fd, char *s_part, char *path) {
#if defined (bios)
    if (false /*booted_from_pxe*/)
        return uri_tftp_dispatch(fd, s_part, path);
#endif

    int partition;

    if (s_part[0] != '\0') {
        uint64_t val = strtoui(s_part, NULL, 10);
        if (val < 1 || val > 256) {
            panic("Partition number outside range 1-256");
        }
        partition = val - 1;
    } else {
        partition = boot_volume->partition;
    }

    struct volume *volume = volume_get_by_coord(boot_volume->drive, partition);
    if (volume == NULL)
        return false;

    if (fopen(fd, volume, path))
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
        panic("bios:// resource is no longer supported. Check CONFIG.md for hdd:// and odd://");
    } else if (!strcmp(resource, "hdd")) {
        ret = uri_hdd_dispatch(fd, root, path);
    } else if (!strcmp(resource, "odd")) {
        ret = uri_odd_dispatch(fd, root, path);
    } else if (!strcmp(resource, "boot")) {
        ret = uri_boot_dispatch(fd, root, path);
    } else if (!strcmp(resource, "guid")) {
        ret = uri_guid_dispatch(fd, root, path);
    } else if (!strcmp(resource, "uuid")) {
        ret = uri_guid_dispatch(fd, root, path);
#if defined (bios)
    } else if (!strcmp(resource, "tftp")) {
        ret = uri_tftp_dispatch(fd, root, path);
#endif
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
