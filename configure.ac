AC_INIT([Limine], [m4_esyscmd([./version.sh])], [https://github.com/limine-bootloader/limine/issues], [limine], [https://limine-bootloader.org/])

AC_PREREQ([2.69])

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST([SRCDIR])
AC_SUBST([BUILDDIR])

. "$SRCDIR"/timestamps

AC_SUBST([REGEN_DATE])
AC_SUBST([SOURCE_DATE_EPOCH])
AC_SUBST([SOURCE_DATE_EPOCH_TOUCH])

AC_CANONICAL_HOST

AC_DEFUN([PROG_ABSPATH], [
    case "$$1" in
        .*) $1="$(cd "$(dirname "$$1")" && pwd -P)/$(basename "$$1")" ;;
    esac
])

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe'

AC_LANG([C])
AC_PROG_CC
PROG_ABSPATH([CC])

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

AC_ARG_VAR([STRIP], [strip command])

if ! test -z "$STRIP"; then
    AC_CHECK_PROG([STRIP_USER_FOUND], [$STRIP], [yes])
    if ! test "x$STRIP_USER_FOUND" = "xyes"; then
        AC_MSG_ERROR([$STRIP not found, please install $STRIP before configuring])
    fi
else
    STRIP="$($CC -dumpmachine)"-strip
    AC_CHECK_PROG([STRIP_FOUND], [$STRIP], [yes])
    if ! test "x$STRIP_FOUND" = "xyes"; then
        STRIP=strip
        AC_CHECK_PROG([STRIP_DEFAULT_FOUND], [$STRIP], [yes])
        if ! test "x$STRIP_DEFAULT_FOUND" = "xyes"; then
            AC_MSG_ERROR([$STRIP not found, please install $STRIP before configuring])
        fi
    fi
fi

AC_SUBST([STRIP])

PKGCONF_LIBS_LIST=""

PKG_PROG_PKG_CONFIG

for lib in $PKGCONF_LIBS_LIST; do
    set -e
    $PKG_CONFIG --exists --print-errors "$lib"
    set +e
done

PKGCONF_CFLAGS="$($PKG_CONFIG --cflags-only-other $PKGCONF_LIBS_LIST 2>/dev/null)"
PKGCONF_CPPFLAGS="$($PKG_CONFIG --cflags-only-I $PKGCONF_LIBS_LIST 2>/dev/null)"
PKGCONF_LIBS="$($PKG_CONFIG --libs $PKGCONF_LIBS_LIST 2>/dev/null)"

AC_SUBST([PKGCONF_CFLAGS])
AC_SUBST([PKGCONF_CPPFLAGS])
AC_SUBST([PKGCONF_LIBS])

OLD_CFLAGS="$CFLAGS"
OLD_CPPFLAGS="$CPPFLAGS"
OLD_LIBS="$LIBS"

CFLAGS="$CFLAGS $PKGCONF_CFLAGS"
CPPFLAGS="$PKGCONF_CPPFLAGS $CPPFLAGS"
LIBS="$LIBS $PKGCONF_LIBS"

AC_CHECK_HEADERS([stdint.h stddef.h stdbool.h limits.h inttypes.h stdio.h stdlib.h string.h],
    [], [AC_MSG_ERROR([required header not found])])

CFLAGS="$OLD_CFLAGS"
CPPFLAGS="$OLD_CPPFLAGS"
LIBS="$OLD_LIBS"

AC_ARG_VAR([TOOLCHAIN_FOR_TARGET], [alternative toolchain prefix for Limine (or 'llvm', or 'gnu')])
AC_ARG_VAR([CC_FOR_TARGET], [C compiler command for Limine])
AC_ARG_VAR([LD_FOR_TARGET], [linker command for Limine])
AC_ARG_VAR([OBJCOPY_FOR_TARGET], [objcopy command for Limine])
AC_ARG_VAR([OBJDUMP_FOR_TARGET], [objdump command for Limine])
AC_ARG_VAR([READELF_FOR_TARGET], [readelf command for Limine])

rm -rf toolchain-files

BUILD_ALL="no"

AC_ARG_ENABLE([all],
    [AS_HELP_STRING([--enable-all], [enable ALL ports and targets])],
    [BUILD_ALL="$enableval"])

BUILD_BIOS_CD="$BUILD_ALL"

AC_ARG_ENABLE([bios-cd],
    [AS_HELP_STRING([--enable-bios-cd], [enable building the x86 BIOS CD image])],
    [BUILD_BIOS_CD="$enableval"])

AC_SUBST([BUILD_BIOS_CD])

BUILD_BIOS_PXE="$BUILD_ALL"

AC_ARG_ENABLE([bios-pxe],
    [AS_HELP_STRING([--enable-bios-pxe], [enable building the x86 BIOS PXE image])],
    [BUILD_BIOS_PXE="$enableval"])

AC_SUBST([BUILD_BIOS_PXE])

BUILD_BIOS="$BUILD_ALL"

AC_ARG_ENABLE([bios],
    [AS_HELP_STRING([--enable-bios], [enable building the x86 BIOS port])],
    [BUILD_BIOS="$enableval"])

if test "x$BUILD_BIOS" = "xno"; then
    if test "x$BUILD_BIOS_CD" = "xyes"; then
        BUILD_BIOS="yes"
    fi
    if test "x$BUILD_BIOS_PXE" = "xyes"; then
        BUILD_BIOS="yes"
    fi
fi

if test "x$BUILD_BIOS" = "xno"; then
    BUILD_BIOS=""
else
    $MKDIR_P toolchain-files
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
        "$SRCDIR/build-aux/freestanding-toolchain/freestanding-toolchain" 2>"toolchain-files/bios-i686-toolchain.mk" || exit 1
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
    $MKDIR_P toolchain-files
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
        "$SRCDIR/build-aux/freestanding-toolchain/freestanding-toolchain" 2>"toolchain-files/uefi-i686-toolchain.mk" || exit 1
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
    $MKDIR_P toolchain-files
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
        "$SRCDIR/build-aux/freestanding-toolchain/freestanding-toolchain" 2>"toolchain-files/uefi-x86_64-toolchain.mk" || exit 1
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
    mkdir -p toolchain-files
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
        "$SRCDIR/build-aux/freestanding-toolchain/freestanding-toolchain" 2>"toolchain-files/uefi-aarch64-toolchain.mk" || exit 1
    BUILD_UEFI_AARCH64="limine-uefi-aarch64"
fi

AC_SUBST([BUILD_UEFI_AARCH64])

BUILD_UEFI_RISCV64="$BUILD_ALL"

AC_ARG_ENABLE([uefi-riscv64],
    [AS_HELP_STRING([--enable-uefi-riscv64], [enable building the riscv64 UEFI port])],
    [BUILD_UEFI_RISCV64="$enableval"])

if test "x$BUILD_UEFI_RISCV64" = "xno"; then
    BUILD_UEFI_RISCV64=""
else
    mkdir -p toolchain-files
    CC="$CC" \
        ARCHITECTURE=riscv64 \
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
        "$SRCDIR/build-aux/freestanding-toolchain/freestanding-toolchain" 2>"toolchain-files/uefi-riscv64-toolchain.mk" || exit 1
    BUILD_UEFI_RISCV64="limine-uefi-riscv64"
fi

AC_SUBST([BUILD_UEFI_RISCV64])

BUILD_UEFI_CD="$BUILD_ALL"

AC_ARG_ENABLE([uefi-cd],
    [AS_HELP_STRING([--enable-uefi-cd], [enable building limine-uefi-cd.bin])],
    [BUILD_UEFI_CD="$enableval"])

if ! test "x$BUILD_UEFI_CD" = "xno"; then
    AC_CHECK_PROG([MTOOLS_FOUND], [mcopy], [yes])
    if ! test "x$MTOOLS_FOUND" = "xyes"; then
        if test "x$BUILD_UEFI_CD" = "xyes"; then
            AC_MSG_ERROR([mtools not found, install mtools to build limine-uefi-cd.bin])
        fi
        AC_MSG_WARN([mtools not found, install mtools to build limine-uefi-cd.bin])
        BUILD_UEFI_CD="no"
    fi
fi

AC_SUBST([BUILD_UEFI_CD])

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

BORROWED_CFLAGS=""
for cflag in $CFLAGS; do
    case $cflag in
        -O*|-pipe|-g|-ffile-prefix-map*)
            BORROWED_CFLAGS="$BORROWED_CFLAGS $cflag"
            ;;
    esac
done

AC_ARG_VAR([CFLAGS_FOR_TARGET], [C flags for Limine])
test "x$CFLAGS_FOR_TARGET" = "x" && CFLAGS_FOR_TARGET="$BORROWED_CFLAGS"

AC_ARG_VAR([CPPFLAGS_FOR_TARGET], [C preprocessor flags for Limine])
test "x$CPPFLAGS_FOR_TARGET" = "x" && CPPFLAGS_FOR_TARGET=""

AC_ARG_VAR([LDFLAGS_FOR_TARGET], [linker flags for Limine])
test "x$LDFLAGS_FOR_TARGET" = "x" && LDFLAGS_FOR_TARGET=""

AC_ARG_VAR([NASMFLAGS_FOR_TARGET], [nasm flags for Limine])
test "x$NASMFLAGS_FOR_TARGET" = "x" && NASMFLAGS_FOR_TARGET="-F dwarf -g"

LIMINE_COPYRIGHT=$($GREP Copyright "$SRCDIR/COPYING")
AC_SUBST([LIMINE_COPYRIGHT])

AC_PREFIX_DEFAULT([/usr/local])

if ! test -f limine.h; then
    cp "$SRCDIR/limine.h" ./
fi

AC_CONFIG_FILES([man/man1/limine.1 GNUmakefile config.h])
AC_OUTPUT
