
#if defined (__APPLE__)

.data
.global __binary_limine_hdd_bin_start
__binary_limine_hdd_bin_start:
    .incbin LIMINE_HDD_BIN
.global __binary_limine_hdd_bin_end
__binary_limine_hdd_bin_end:

#elif defined (__MINGW32__)

.section .data
.global __binary_limine_hdd_bin_start
__binary_limine_hdd_bin_start:
    .incbin LIMINE_HDD_BIN
.global __binary_limine_hdd_bin_end
__binary_limine_hdd_bin_end:

#elif defined (__linux__) || defined (__unix__)

.section .data
.global _binary_limine_hdd_bin_start
_binary_limine_hdd_bin_start:
    .incbin LIMINE_HDD_BIN
.global _binary_limine_hdd_bin_end
_binary_limine_hdd_bin_end:

#endif
