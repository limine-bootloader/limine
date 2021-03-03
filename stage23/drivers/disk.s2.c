#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#if defined (bios)
#  include <lib/real.h>
#elif defined (uefi)
#  include <efi.h>
#endif
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

#if defined(bios)

struct bios_drive_params {
    uint16_t buf_size;
    uint16_t info_flags;
    uint32_t cyl;
    uint32_t heads;
    uint32_t sects;
    uint64_t lba_count;
    uint16_t bytes_per_sect;
    uint32_t edd;
} __attribute__((packed));

struct dap {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
};

static struct dap *dap = NULL;

#define XFER_BUF_SIZE 16384
static void *xfer_buf = NULL;

bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count) {
    if (count * volume->sector_size > XFER_BUF_SIZE)
        panic("XFER");

    if (xfer_buf == NULL)
        xfer_buf = conv_mem_alloc_aligned(XFER_BUF_SIZE, 16);

    if (dap == NULL) {
        dap = conv_mem_alloc(sizeof(struct dap));
        dap->size = 16;
    }

    dap->count = count;

    dap->segment = rm_seg(xfer_buf);
    dap->offset  = rm_off(xfer_buf);
    dap->lba     = block;

    struct rm_regs r = {0};
    r.eax = 0x4200;
    r.edx = volume->drive;
    r.esi = (uint32_t)rm_off(dap);
    r.ds  = rm_seg(dap);

    rm_int(0x13, &r, &r);

    if (r.eflags & EFLAGS_CF) {
        int ah = (r.eax >> 8) & 0xff;
        switch (ah) {
            case 0x0c:
                return false;
            default:
                panic("Disk error %x. Drive %x, LBA %x.",
                      ah, volume->drive, dap->lba);
        }
    }

    if (buf != NULL)
        memcpy(buf, xfer_buf, count * volume->sector_size);

    return true;
}

size_t disk_create_index(struct volume **ret) {
    struct volume *volume_index;
    size_t volume_count = 0, volume_index_i = 0;

    for (uint8_t drive = 0x80; drive; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        print("Found BIOS drive %x\n", drive);
        print(" ... %X total %u-byte sectors\n",
              drive_params.lba_count, drive_params.bytes_per_sect);

        volume_count++;

        struct volume block = {0};

        block.drive = drive;
        block.sector_size = drive_params.bytes_per_sect;
        block.first_sect = 0;
        block.sect_count = drive_params.lba_count;

        // The medium could not be present (e.g.: CD-ROMs)
        // Do a test run to see if we can actually read it
        if (!disk_read_sectors(&block, NULL, 0, 1)) {
            print(" ... Ignoring drive...\n");
            continue;
        }

        for (int part = 0; ; part++) {
            struct volume p = {0};
            int ret = part_get(&p, &block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_count++;
        }
    }

    volume_index = ext_mem_alloc(sizeof(struct volume) * volume_count);

    for (uint8_t drive = 0x80; drive; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        struct volume *block = &volume_index[volume_index_i++];

        block->drive = drive;
        block->partition = -1;
        block->sector_size = drive_params.bytes_per_sect;
        block->first_sect = 0;
        block->sect_count = drive_params.lba_count;

        // The medium could not be present (e.g.: CD-ROMs)
        // Do a test run to see if we can actually read it
        if (!disk_read_sectors(block, NULL, 0, 1)) {
            continue;
        }

        if (gpt_get_guid(&block->guid, block)) {
            block->guid_valid = true;
        }

        for (int part = 0; ; part++) {
            struct volume p = {0};
            int ret = part_get(&p, block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_index[volume_index_i++] = p;
        }
    }

    *ret = volume_index;
    return volume_count;
}

#endif

#if defined (uefi)

bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count) {
    EFI_STATUS status;

    status = uefi_call_wrapper(volume->drive->ReadBlocks, 5, volume->drive,
                               volume->drive->Media->MediaId,
                               block, count * volume->sector_size, buf);

    if (status != 0) {
        return false;
    }

    return true;
}

size_t disk_create_index(struct volume **ret) {
    EFI_STATUS status;

    struct volume *volume_index = NULL;
    size_t volume_index_i = 0;
    size_t volume_count = 0;

    EFI_GUID block_io_guid = BLOCK_IO_PROTOCOL;
    EFI_HANDLE *handles = NULL;
    UINTN handles_size = 0;

    uefi_call_wrapper(gBS->LocateHandle, 5, ByProtocol, &block_io_guid,
                      NULL, &handles_size, handles);

    handles = ext_mem_alloc(handles_size);

    uefi_call_wrapper(gBS->LocateHandle, 5, ByProtocol, &block_io_guid,
                      NULL, &handles_size, handles);

    for (size_t i = 0; i < handles_size / sizeof(EFI_HANDLE); i++) {
        struct volume block = {0};

        status = uefi_call_wrapper(gBS->HandleProtocol, 3, handles[i],
                                   &block_io_guid, &block.drive);

        if (status != 0 || block.drive == NULL || block.drive->Media->LastBlock == 0)
            continue;

        if (block.drive->Media->LogicalPartition)
            continue;

        volume_count++;

        block.sector_size = block.drive->Media->BlockSize;
        block.first_sect = 0;
        block.sect_count = block.drive->Media->LastBlock + 1;

        for (int part = 0; ; part++) {
            struct volume p = {0};
            int ret = part_get(&p, &block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_count++;
        }
    }

    volume_index = ext_mem_alloc(sizeof(struct volume) * volume_count);

    for (size_t i = 0; i < handles_size / sizeof(EFI_HANDLE); i++) {
        EFI_BLOCK_IO *drive;

        status = uefi_call_wrapper(gBS->HandleProtocol, 3, handles[i],
                                   &block_io_guid, &drive);

        if (status != 0 || drive == NULL || drive->Media->LastBlock == 0)
            continue;

        if (drive->Media->LogicalPartition)
            continue;

        struct volume *block = &volume_index[volume_index_i++];

        block->drive = drive;
        block->partition = -1;
        block->sector_size = drive->Media->BlockSize;
        block->first_sect = 0;
        block->sect_count = drive->Media->LastBlock + 1;

        for (int part = 0; ; part++) {
            struct volume p = {0};
            int ret = part_get(&p, block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_index[volume_index_i++] = p;
        }
    }

    *ret = volume_index;
    return volume_count;
}

#endif
