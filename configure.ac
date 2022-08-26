AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine], [https://limine-bootloader.org/])

AC_PREREQ(2.69)

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SUBDIRS([cross-detect])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST(SRCDIR)
AC_SUBST(BUILDDIR)

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe -Wall -Wextra'
AC_PROG_CC

werror_state="no"
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [treat warnings as errors]),
    werror_state="$enableval")

if test "$werror_state" = "yes"; then
    AC_SUBST(WERROR, [-Werror])
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
AC_PROG_GREP
AC_PROG_SED
AC_PROG_AWK

AC_CHECK_PROG([FIND_FOUND], [find], [yes])
if ! test "x$FIND_FOUND" = "xyes"; then
    AC_MSG_ERROR([find not found, please install find before configuring])
fi

AC_CHECK_PROG([NASM_FOUND], [nasm], [yes])
if ! test "x$NASM_FOUND" = "xyes"; then
    AC_MSG_ERROR([nasm not found, please install nasm before configuring])
fi

AC_CHECK_PROG([GZIP_FOUND], [gzip], [yes])
if ! test "x$GZIP_FOUND" = "xyes"; then
    AC_MSG_ERROR([gzip not found, please install gzip before configuring])
fi

rm -rf "$BUILDDIR/cross-files"

BUILD_ALL="no"

AC_ARG_ENABLE([all],
    AS_HELP_STRING([--enable-all], [enable ALL ports]),
    BUILD_ALL="$enableval")

BUILD_BIOS="$BUILD_ALL"

AC_ARG_ENABLE([bios],
    AS_HELP_STRING([--enable-bios], [enable building the x86 BIOS port]),
    BUILD_BIOS="$enableval")

if test "x$BUILD_BIOS" = "xno"; then
    BUILD_BIOS=""
else
    (
    $MKDIR_P "$BUILDDIR/cross-files"
    cd "$BUILDDIR/cross-files"
    TRIPLET=i686-elf "$SRCDIR/cross-detect/configure"
    ) || exit 1
    BUILD_BIOS="limine-bios"
fi

AC_SUBST(BUILD_BIOS)

BUILD_LIMINE_DEPLOY="$BUILD_ALL"

AC_ARG_ENABLE([limine-deploy],
    AS_HELP_STRING([--enable-limine-deploy], [enable building limine-deploy]),
    BUILD_LIMINE_DEPLOY="$enableval")

AC_SUBST(BUILD_LIMINE_DEPLOY)

BUILD_CD="$BUILD_ALL"

AC_ARG_ENABLE([bios-cd],
    AS_HELP_STRING([--enable-bios-cd], [enable building the x86 BIOS CD image]),
    BUILD_CD="$enableval")

AC_SUBST(BUILD_CD)

BUILD_PXE="$BUILD_ALL"

AC_ARG_ENABLE([bios-pxe],
    AS_HELP_STRING([--enable-bios-pxe], [enable building the x86 BIOS PXE image]),
    BUILD_PXE="$enableval")

AC_SUBST(BUILD_PXE)

BUILD_UEFI_IA32="$BUILD_ALL"

AC_ARG_ENABLE([uefi-ia32],
    AS_HELP_STRING([--enable-uefi-ia32], [enable building the IA-32 UEFI port]),
    BUILD_UEFI_IA32="$enableval")

if test "x$BUILD_UEFI_IA32" = "xno"; then
    BUILD_UEFI_IA32=""
else
    (
    $MKDIR_P "$BUILDDIR/cross-files"
    cd "$BUILDDIR/cross-files"
    TRIPLET=i686-elf "$SRCDIR/cross-detect/configure"
    ) || exit 1
    BUILD_UEFI_IA32="limine-uefi-ia32"
fi

AC_SUBST(BUILD_UEFI_IA32)

BUILD_UEFI_X86_64="$BUILD_ALL"

AC_ARG_ENABLE([uefi-x86-64],
    AS_HELP_STRING([--enable-uefi-x86_64], [enable building the x86_64 UEFI port]),
    BUILD_UEFI_X86_64="$enableval")

if test "x$BUILD_UEFI_X86_64" = "xno"; then
    BUILD_UEFI_X86_64=""
else
    (
    $MKDIR_P "$BUILDDIR/cross-files"
    cd "$BUILDDIR/cross-files"
    TRIPLET=x86_64-elf "$SRCDIR/cross-detect/configure"
    ) || exit 1
    BUILD_UEFI_X86_64="limine-uefi-x86-64"
fi

AC_SUBST(BUILD_UEFI_X86_64)

BUILD_CD_EFI="$BUILD_ALL"

AC_ARG_ENABLE([cd-efi],
    AS_HELP_STRING([--enable-cd-efi], [enable building limine-cd-efi.bin]),
    BUILD_CD_EFI="$enableval")

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

AC_SUBST(BUILD_CD_EFI)

AC_ARG_VAR(CROSS_TOOLCHAIN, [alternative toolchain prefix (or 'llvm')])
AC_ARG_VAR(CROSS_CC, [C compiler command for Limine])
AC_ARG_VAR(CROSS_LD, [linker command for Limine])
AC_ARG_VAR(CROSS_OBJCOPY, [objcopy command for Limine])
AC_ARG_VAR(CROSS_OBJDUMP, [objdump command for Limine])
AC_ARG_VAR(CROSS_READELF, [readelf command for Limine])

m4_define([DEFAULT_CROSS_CFLAGS], [-O3 -pipe -Wall -Wextra])
AC_ARG_VAR(CROSS_CFLAGS, [C flags for Limine @<:@default: ]DEFAULT_CROSS_CFLAGS[@:>@])
test "x$CROSS_CFLAGS" = "x" && CROSS_CFLAGS="DEFAULT_CROSS_CFLAGS"

m4_define([DEFAULT_CROSS_LDFLAGS], [])
AC_ARG_VAR(CROSS_LDFLAGS, [Linker flags for Limine @<:@default: ]DEFAULT_CROSS_LDFLAGS[@:>@])
test "x$CROSS_LDFLAGS" = "x" && CROSS_LDFLAGS="DEFAULT_CROSS_LDFLAGS"

LIMINE_COPYRIGHT=$($GREP Copyright "$SRCDIR/LICENSE.md")
AC_SUBST(LIMINE_COPYRIGHT)

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile config.h)
AC_OUTPUT
