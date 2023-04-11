AC_INIT([Limine], [m4_esyscmd([./version.sh])], [https://github.com/limine-bootloader/limine/issues], [limine], [https://limine-bootloader.org/])

AC_PREREQ([2.69])

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST([SRCDIR])
AC_SUBST([BUILDDIR])

REGEN_DATE="m4_esyscmd([date '+%B %Y' | tr -d '\n'])"
AC_SUBST([REGEN_DATE])

AC_DEFUN([PROG_ABSPATH], [
    case "$$1" in
        .*) $1="$(cd "$(dirname "$$1")" && pwd -P)/$(basename "$$1")" ;;
    esac
])

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe'

AC_LANG([C])
AC_PROG_CC
PROG_ABSPATH([CC])

AC_CHECK_HEADERS([stdint.h stddef.h stdbool.h limits.h inttypes.h stdio.h stdlib.h string.h],
    [], [AC_MSG_ERROR([required header not found])])

werror_state="no"
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [treat warnings as errors])],
    [werror_state="$enableval"])
if test "$werror_state" = "yes"; then
    AC_SUBST([WERROR_FLAG], [-Werror])
else
    AC_SUBST([WERROR_FLAG], [-Wno-error])
fi

AC_PROG_MKDIR_P
PROG_ABSPATH([MKDIR_P])
AC_PROG_INSTALL
PROG_ABSPATH([INSTALL])
AC_PROG_SED
PROG_ABSPATH([SED])
AC_PROG_GREP
PROG_ABSPATH([GREP])
AC_PROG_AWK
PROG_ABSPATH([AWK])

AC_CHECK_PROG([FIND_FOUND], [find], [yes])
if ! test "x$FIND_FOUND" = "xyes"; then
    AC_MSG_ERROR([find not found, please install find before configuring])
fi

NATIVE_STRIP="$($CC -dumpmachine)"-strip
AC_CHECK_PROG([NATIVE_STRIP_FOUND], [$NATIVE_STRIP], [yes])
if ! test "x$NATIVE_STRIP_FOUND" = "xyes"; then
    NATIVE_STRIP=strip
fi
AC_SUBST([NATIVE_STRIP])

rm -rf "$BUILDDIR/toolchain-files"

BUILD_ALL="no"

AC_ARG_ENABLE([all],
    [AS_HELP_STRING([--enable-all], [enable ALL ports and targets])],
    [BUILD_ALL="$enableval"])

BUILD_CD="$BUILD_ALL"

AC_ARG_ENABLE([bios-cd],
    [AS_HELP_STRING([--enable-bios-cd], [enable building the x86 BIOS CD image])],
    [BUILD_CD="$enableval"])

AC_SUBST([BUILD_CD])

BUILD_PXE="$BUILD_ALL"

AC_ARG_ENABLE([bios-pxe],
    [AS_HELP_STRING([--enable-bios-pxe], [enable building the x86 BIOS PXE image])],
    [BUILD_PXE="$enableval"])

AC_SUBST([BUILD_PXE])

BUILD_BIOS="$BUILD_ALL"

AC_ARG_ENABLE([bios],
    [AS_HELP_STRING([--enable-bios], [enable building the x86 BIOS port])],
    [BUILD_BIOS="$enableval"])

if test "x$BUILD_BIOS" = "xno"; then
    if test "x$BUILD_CD" = "xyes"; then
        BUILD_BIOS="yes"
    fi
    if test "x$BUILD_PXE" = "xyes"; then
        BUILD_BIOS="yes"
    fi
fi

if test "x$BUILD_BIOS" = "xno"; then
    BUILD_BIOS=""
else
    $MKDIR_P "$BUILDDIR/toolchain-files"
    CC="$CC" \
        ARCHITECTURE=i686 \
        FREESTANDING_TOOLCHAIN_SUFFIX="_FOR_TARGET" \
        FREESTANDING_TOOLCHAIN="$TOOLCHAIN_FOR_TARGET" \
        WANT_FREESTANDING_CC=yes \
        FREESTANDING_CC="$CC_FOR_TARGET" \
        WANT_FREESTANDING_LD=yes \
        FREESTANDING_LD="$LD_FOR_TARGET" \
        WANT_FREESTANDING_OBJCOPY=yes \
        FREESTANDING_OBJCOPY="$OBJCOPY_FOR_TARGET" \
        WANT_FREESTANDING_OBJDUMP=yes \
        FREESTANDING_OBJDUMP="$OBJDUMP_FOR_TARGET" \
        WANT_FREESTANDING_READELF=yes \
        FREESTANDING_READELF="$READELF_FOR_TARGET" \
        "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/bios-i686-toolchain.mk" || exit 1
    BUILD_BIOS="limine-bios"
    NEED_NASM=yes
    NEED_GZIP=yes
fi

AC_SUBST([BUILD_BIOS])

BUILD_UEFI_IA32="$BUILD_ALL"

AC_ARG_ENABLE([uefi-ia32],
    [AS_HELP_STRING([--enable-uefi-ia32], [enable building the IA-32 UEFI port])],
    [BUILD_UEFI_IA32="$enableval"])

if test "x$BUILD_UEFI_IA32" = "xno"; then
    BUILD_UEFI_IA32=""
else
    $MKDIR_P "$BUILDDIR/toolchain-files"
    CC="$CC" \
        ARCHITECTURE=i686 \
        FREESTANDING_TOOLCHAIN_SUFFIX="_FOR_TARGET" \
        FREESTANDING_TOOLCHAIN="$TOOLCHAIN_FOR_TARGET" \
        WANT_FREESTANDING_CC=yes \
        FREESTANDING_CC="$CC_FOR_TARGET" \
        WANT_FREESTANDING_LD=yes \
        FREESTANDING_LD="$LD_FOR_TARGET" \
        WANT_FREESTANDING_OBJCOPY=yes \
        FREESTANDING_OBJCOPY="$OBJCOPY_FOR_TARGET" \
        WANT_FREESTANDING_OBJDUMP=yes \
        FREESTANDING_OBJDUMP="$OBJDUMP_FOR_TARGET" \
        "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-i686-toolchain.mk" || exit 1
    BUILD_UEFI_IA32="limine-uefi-ia32"
    NEED_NASM=yes
fi

AC_SUBST([BUILD_UEFI_IA32])

BUILD_UEFI_X86_64="$BUILD_ALL"

AC_ARG_ENABLE([uefi-x86-64],
    [AS_HELP_STRING([--enable-uefi-x86-64], [enable building the x86-64 UEFI port])],
    [BUILD_UEFI_X86_64="$enableval"])

if test "x$BUILD_UEFI_X86_64" = "xno"; then
    BUILD_UEFI_X86_64=""
else
    $MKDIR_P "$BUILDDIR/toolchain-files"
    CC="$CC" \
        ARCHITECTURE=x86_64 \
        FREESTANDING_TOOLCHAIN_SUFFIX="_FOR_TARGET" \
        FREESTANDING_TOOLCHAIN="$TOOLCHAIN_FOR_TARGET" \
        WANT_FREESTANDING_CC=yes \
        FREESTANDING_CC="$CC_FOR_TARGET" \
        WANT_FREESTANDING_LD=yes \
        FREESTANDING_LD="$LD_FOR_TARGET" \
        WANT_FREESTANDING_OBJCOPY=yes \
        FREESTANDING_OBJCOPY="$OBJCOPY_FOR_TARGET" \
        WANT_FREESTANDING_OBJDUMP=yes \
        FREESTANDING_OBJDUMP="$OBJDUMP_FOR_TARGET" \
        "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-x86_64-toolchain.mk" || exit 1
    BUILD_UEFI_X86_64="limine-uefi-x86-64"
    NEED_NASM=yes
fi

AC_SUBST([BUILD_UEFI_X86_64])

BUILD_UEFI_AARCH64="$BUILD_ALL"

AC_ARG_ENABLE([uefi-aarch64],
    [AS_HELP_STRING([--enable-uefi-aarch64], [enable building the aarch64 UEFI port])],
    [BUILD_UEFI_AARCH64="$enableval"])

if test "x$BUILD_UEFI_AARCH64" = "xno"; then
    BUILD_UEFI_AARCH64=""
else
    mkdir -p "$BUILDDIR/toolchain-files"
    CC="$CC" \
        ARCHITECTURE=aarch64 \
        FREESTANDING_TOOLCHAIN_SUFFIX="_FOR_TARGET" \
        FREESTANDING_TOOLCHAIN="$TOOLCHAIN_FOR_TARGET" \
        WANT_FREESTANDING_CC=yes \
        FREESTANDING_CC="$CC_FOR_TARGET" \
        WANT_FREESTANDING_LD=yes \
        FREESTANDING_LD="$LD_FOR_TARGET" \
        WANT_FREESTANDING_OBJCOPY=yes \
        FREESTANDING_OBJCOPY="$OBJCOPY_FOR_TARGET" \
        WANT_FREESTANDING_OBJDUMP=yes \
        FREESTANDING_OBJDUMP="$OBJDUMP_FOR_TARGET" \
        "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-aarch64-toolchain.mk" || exit 1
    BUILD_UEFI_AARCH64="limine-uefi-aarch64"
fi

AC_SUBST([BUILD_UEFI_AARCH64])

BUILD_CD_EFI="$BUILD_ALL"

AC_ARG_ENABLE([uefi-cd],
    [AS_HELP_STRING([--enable-uefi-cd], [enable building limine-cd-efi.bin])],
    [BUILD_CD_EFI="$enableval"])

if ! test "x$BUILD_CD_EFI" = "xno"; then
    AC_CHECK_PROG([MTOOLS_FOUND], [mcopy], [yes])
    if ! test "x$MTOOLS_FOUND" = "xyes"; then
        if test "x$BUILD_CD_EFI" = "xyes"; then
            AC_MSG_ERROR([mtools not found, install mtools to build limine-cd-efi.bin])
        fi
        AC_MSG_WARN([mtools not found, install mtools to build limine-cd-efi.bin])
        BUILD_CD_EFI="no"
    fi
fi

AC_SUBST([BUILD_CD_EFI])

if test "x$NEED_NASM" = "xyes"; then
    AC_CHECK_PROG([NASM_FOUND], [nasm], [yes])
    if ! test "x$NASM_FOUND" = "xyes"; then
        AC_MSG_ERROR([nasm not found, please install nasm before configuring])
    fi
fi

if test "x$NEED_GZIP" = "xyes"; then
    AC_CHECK_PROG([GZIP_FOUND], [gzip], [yes])
    if ! test "x$GZIP_FOUND" = "xyes"; then
        AC_MSG_ERROR([gzip not found, please install gzip before configuring])
    fi
fi

AC_ARG_VAR([TOOLCHAIN_FOR_TARGET], [alternative toolchain prefix for Limine (or 'llvm', or 'gnu')])
AC_ARG_VAR([CC_FOR_TARGET], [C compiler command for Limine])
AC_ARG_VAR([LD_FOR_TARGET], [linker command for Limine])
AC_ARG_VAR([OBJCOPY_FOR_TARGET], [objcopy command for Limine])
AC_ARG_VAR([OBJDUMP_FOR_TARGET], [objdump command for Limine])
AC_ARG_VAR([READELF_FOR_TARGET], [readelf command for Limine])

m4_define([DEFAULT_CFLAGS_FOR_TARGET], [-g -O2 -pipe])
AC_ARG_VAR([CFLAGS_FOR_TARGET], [C flags for Limine @<:@default: ]DEFAULT_CFLAGS_FOR_TARGET[@:>@])
test "x$CFLAGS_FOR_TARGET" = "x" && CFLAGS_FOR_TARGET="DEFAULT_CFLAGS_FOR_TARGET"

m4_define([DEFAULT_CPPFLAGS_FOR_TARGET], [])
AC_ARG_VAR([CPPFLAGS_FOR_TARGET], [C preprocessor flags for Limine @<:@default: ]DEFAULT_CPPFLAGS_FOR_TARGET[@:>@])
test "x$CPPFLAGS_FOR_TARGET" = "x" && CPPFLAGS_FOR_TARGET="DEFAULT_CPPFLAGS_FOR_TARGET"

m4_define([DEFAULT_LDFLAGS_FOR_TARGET], [])
AC_ARG_VAR([LDFLAGS_FOR_TARGET], [Linker flags for Limine @<:@default: ]DEFAULT_LDFLAGS_FOR_TARGET[@:>@])
test "x$LDFLAGS_FOR_TARGET" = "x" && LDFLAGS_FOR_TARGET="DEFAULT_LDFLAGS_FOR_TARGET"

LIMINE_COPYRIGHT=$($GREP Copyright "$SRCDIR/LICENSE")
AC_SUBST([LIMINE_COPYRIGHT])

AC_PREFIX_DEFAULT([/usr/local])

if ! test "$SRCDIR" = "$BUILDDIR"; then
    cp "$SRCDIR/limine.h" "$BUILDDIR/"
fi

AC_CONFIG_FILES([man/man1/limine.1 GNUmakefile config.h])
AC_OUTPUT
