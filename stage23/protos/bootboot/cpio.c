#include "lib/print.h"
#include <protos/bootboot/initrd.h>
#include <lib/libc.h>
#include <stdint.h>

#define HPODC_MAGIC "070707"

/**
 * cpio archive
 */
INITRD_HANDLER(cpio) {
    /// Some macros///
#define _offset(cnt) do { offset += (cnt); if (offset > file.size) return (file_t){ 0, NULL }; } while (false);
#define _atoffset() (&file.data[offset])
#define _must(n) do { if ((offset + (n)) > file.size) return (file_t){ 0, NULL }; } while (false);
    uint64_t offset = 0;
    if (file.size < 6) return (file_t){ 0, NULL };

    /// Check magic ///
    // this may be a bit unclear, but this checks if the file even **has** a cpio header
    if (memcmp(file.data,"070701",6) && memcmp(file.data, "070702", 6) && memcmp(file.data, "070707", 6))
        return (file_t){ 0, NULL };
    
    /// Some variables ///
    uint64_t path_name_size = strlen(path);

    /// hpodc archive ///
    while (!memcmp(_atoffset(), HPODC_MAGIC, 6)) {
        _must(22);
        uint32_t name_size = oct2bin(_atoffset()+ 8 * 6 + 11, 6);
        uint32_t file_size = oct2bin(_atoffset()+ 8 * 6 + 11 + 6, 11);
        _must(9 * 6 + 2 * 11 + name_size);

        uint8_t* target_path = _atoffset() + 9 * 6 + 2 * 11;

        if (name_size > 2 && target_path[0] == '.' && target_path[1] == '/') {
            target_path += 2;
        }

        if (!memcmp(target_path, path, path_name_size + 1)) {
            return (file_t){
                file_size,
                _atoffset() + 9 * 6 + 2 * 11 + name_size,
            };
        }
        _offset(76 + name_size + file_size);
    }
    offset = 0;
    // newc and crc archive
    while(!memcmp(_atoffset(), "07070", 5)){
        uint32_t file_size = hex2bin(_atoffset() + 8 * 6 + 6, 8);
        uint32_t name_size = hex2bin(_atoffset() + 8 * 11 + 6, 8);

        uint8_t* target_path = _atoffset() + 110;

        if (name_size > 2 && target_path[0] == '.' && target_path[1] == '/') {
            target_path += 2;
        }
        
        if (!memcmp(target_path, path, path_name_size + 1)) {
            uint8_t buf[9];
            memcpy(buf, _atoffset() + 8 * 11 + 6, 8);
            buf[8] = 0;
            return (file_t){
                file_size,
                (_atoffset() + ((110 + name_size + 3) / 4) * 4),
            };
        }
        _offset(((110 + name_size + 3) / 4) * 4 + ((file_size + 3) / 4) * 4);
    }
    char buf[9];
    memcpy(buf, _atoffset(), 8);
    buf[8] = 0;
    return (file_t){ 0, NULL };
}