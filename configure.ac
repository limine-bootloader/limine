AC_INIT([Limine], [m4_esyscmd([./version.sh])], [https://github.com/limine-bootloader/limine/issues], [limine], [https://limine-bootloader.org/])

AC_PREREQ([2.69])

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST([SRCDIR])
AC_SUBST([BUILDDIR])

REGEN_DATE="m4_esyscmd([date '+%B %Y' | tr -d '\n'])"
AC_SUBST([REGEN_DATE])

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe -Wall -Wextra'

AC_LANG([C])
AC_PROG_CC

AC_CHECK_HEADERS([stdint.h stddef.h stdbool.h limits.h inttypes.h stdio.h stdlib.h string.h],
    [], [AC_MSG_ERROR([required header not found])])

werror_state="no"
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [treat warnings as errors])],
    [werror_state="$enableval"])
if test "$werror_state" = "yes"; then
    AC_SUBST([WERROR], [-Werror])
    CFLAGS="$CFLAGS -Werror"
fi

AC_PROG_MKDIR_P
case "$MKDIR_P" in
    .*) MKDIR_P="$(cd "$(dirname "$MKDIR_P")" && pwd -P)/$(basename "$MKDIR_P")" ;;
esac
AC_PROG_INSTALL
case "$INSTALL" in
    .*) INSTALL="$(cd "$(dirname "$INSTALL")" && pwd -P)/$(basename "$INSTALL")" ;;
esac
AC_PROG_SED
AC_PROG_GREP
AC_PROG_AWK

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

BUILD_LIMINE_DEPLOY="$BUILD_BIOS"

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
    CC="$CC" ARCHITECTURE=i686 WANT_FREESTANDING_CC=yes WANT_FREESTANDING_LD=yes WANT_FREESTANDING_OBJCOPY=yes WANT_FREESTANDING_OBJDUMP=yes WANT_FREESTANDING_READELF=yes "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/bios-i686-toolchain.mk"
    BUILD_BIOS="limine-bios"
    NEED_NASM=yes
    NEED_GZIP=yes
fi

AC_SUBST([BUILD_BIOS])
AC_SUBST([BUILD_LIMINE_DEPLOY])

BUILD_UEFI_IA32="$BUILD_ALL"

AC_ARG_ENABLE([uefi-ia32],
    [AS_HELP_STRING([--enable-uefi-ia32], [enable building the IA-32 UEFI port])],
    [BUILD_UEFI_IA32="$enableval"])

if test "x$BUILD_UEFI_IA32" = "xno"; then
    BUILD_UEFI_IA32=""
else
    $MKDIR_P "$BUILDDIR/toolchain-files"
    CC="$CC" ARCHITECTURE=i686 WANT_FREESTANDING_CC=yes WANT_FREESTANDING_LD=yes WANT_FREESTANDING_OBJCOPY=yes WANT_FREESTANDING_OBJDUMP=yes "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-i686-toolchain.mk"
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
    CC="$CC" ARCHITECTURE=x86_64 WANT_FREESTANDING_CC=yes WANT_FREESTANDING_LD=yes WANT_FREESTANDING_OBJCOPY=yes WANT_FREESTANDING_OBJDUMP=yes "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-x86_64-toolchain.mk"
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
    CC="$CC" ARCHITECTURE=aarch64 WANT_FREESTANDING_CC=yes WANT_FREESTANDING_LD=yes WANT_FREESTANDING_OBJCOPY=yes WANT_FREESTANDING_OBJDUMP=yes "$SRCDIR/freestanding-toolchain" >"$BUILDDIR/toolchain-files/uefi-aarch64-toolchain.mk"
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

AC_ARG_VAR([FREESTANDING_TOOLCHAIN], [alternative toolchain prefix for Limine (or 'llvm', or 'gnu')])
AC_ARG_VAR([FREESTANDING_CC], [C compiler command for Limine])
AC_ARG_VAR([FREESTANDING_LD], [linker command for Limine])
AC_ARG_VAR([FREESTANDING_OBJCOPY], [objcopy command for Limine])
AC_ARG_VAR([FREESTANDING_OBJDUMP], [objdump command for Limine])
AC_ARG_VAR([FREESTANDING_READELF], [readelf command for Limine])

m4_define([DEFAULT_FREESTANDING_CFLAGS], [-g -O2 -pipe -Wall -Wextra])
AC_ARG_VAR([FREESTANDING_CFLAGS], [C flags for Limine @<:@default: ]DEFAULT_FREESTANDING_CFLAGS[@:>@])
test "x$FREESTANDING_CFLAGS" = "x" && FREESTANDING_CFLAGS="DEFAULT_FREESTANDING_CFLAGS"
if test "$werror_state" = "yes"; then
    FREESTANDING_CFLAGS="$FREESTANDING_CFLAGS -Werror"
fi

m4_define([DEFAULT_FREESTANDING_CPPFLAGS], [])
AC_ARG_VAR([FREESTANDING_CPPFLAGS], [C preprocessor flags for Limine @<:@default: ]DEFAULT_FREESTANDING_CPPFLAGS[@:>@])
test "x$FREESTANDING_CPPFLAGS" = "x" && FREESTANDING_CPPFLAGS="DEFAULT_FREESTANDING_CPPFLAGS"

m4_define([DEFAULT_FREESTANDING_LDFLAGS], [])
AC_ARG_VAR([FREESTANDING_LDFLAGS], [Linker flags for Limine @<:@default: ]DEFAULT_FREESTANDING_LDFLAGS[@:>@])
test "x$FREESTANDING_LDFLAGS" = "x" && FREESTANDING_LDFLAGS="DEFAULT_FREESTANDING_LDFLAGS"

LIMINE_COPYRIGHT=$($GREP Copyright "$SRCDIR/LICENSE")
AC_SUBST([LIMINE_COPYRIGHT])

AC_PREFIX_DEFAULT([/usr/local])

AC_CONFIG_FILES([man/man1/limine-version.1 man/man1/limine-deploy.1 man/man1/limine-enroll-config.1 GNUmakefile config.h])
AC_OUTPUT
