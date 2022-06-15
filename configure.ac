AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine], [https://limine-bootloader.org/])

AC_PREREQ(2.69)

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"
PATH="$BUILDDIR/toolchain/bin$PATH_SEPARATOR$SRCDIR/toolchain/bin$PATH_SEPARATOR/usr/local/bin$PATH_SEPARATOR$PATH"
export PATH

AC_SUBST(SRCDIR, [$SRCDIR])
AC_SUBST(BUILDDIR, [$BUILDDIR])
AC_SUBST(PATH, [$PATH])

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

AC_PROG_INSTALL
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

BUILD_BIOS="default"

AC_ARG_ENABLE([bios],
    AS_HELP_STRING([--disable-bios], [disable building the x86 BIOS port]),
    BUILD_BIOS="$enableval")

if test "x$BUILD_BIOS" = "xno"; then
    BUILD_BIOS=""
else
    BUILD_BIOS="limine-bios"
fi

AC_SUBST(BUILD_BIOS, [$BUILD_BIOS])

BUILD_LIMINE_DEPLOY="default"

AC_ARG_ENABLE([limine-deploy],
    AS_HELP_STRING([--disable-limine-deploy], [disable building limine-deploy]),
    BUILD_LIMINE_DEPLOY="$enableval")

AC_SUBST(BUILD_LIMINE_DEPLOY, [$BUILD_LIMINE_DEPLOY])

BUILD_CD="default"

AC_ARG_ENABLE([bios-cd],
    AS_HELP_STRING([--disable-bios-cd], [disable building the x86 BIOS CD image]),
    BUILD_CD="$enableval")

AC_SUBST(BUILD_CD, [$BUILD_CD])

BUILD_PXE="default"

AC_ARG_ENABLE([bios-pxe],
    AS_HELP_STRING([--disable-bios-pxe], [disable building the x86 BIOS PXE image]),
    BUILD_PXE="$enableval")

AC_SUBST(BUILD_PXE, [$BUILD_PXE])

BUILD_UEFI_IA32="default"

AC_ARG_ENABLE([uefi-ia32],
    AS_HELP_STRING([--disable-uefi-ia32], [disable building the IA-32 UEFI port]),
    BUILD_UEFI_IA32="$enableval")

if test "x$BUILD_UEFI_IA32" = "xno"; then
    BUILD_UEFI_IA32=""
else
    BUILD_UEFI_IA32="limine-uefi-ia32"
fi

AC_SUBST(BUILD_UEFI_IA32, [$BUILD_UEFI_IA32])

BUILD_UEFI_X86_64="default"

AC_ARG_ENABLE([uefi-x86-64],
    AS_HELP_STRING([--disable-uefi-x86_64], [disable building the x86_64 UEFI port]),
    BUILD_UEFI_X86_64="$enableval")

if test "x$BUILD_UEFI_X86_64" = "xno"; then
    BUILD_UEFI_X86_64=""
else
    BUILD_UEFI_X86_64="limine-uefi-x86-64"
fi

AC_SUBST(BUILD_UEFI_X86_64, [$BUILD_UEFI_X86_64])

BUILD_CD_EFI="default"

AC_ARG_ENABLE([cd-efi],
    AS_HELP_STRING([--disable-cd-efi], [disable limine-cd-efi.bin build]),
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

AC_SUBST(BUILD_CD_EFI, [$BUILD_CD_EFI])

AC_ARG_VAR(TOOLCHAIN, [Alternative toolchain prefix (or 'llvm') [default: limine]])
test "x$TOOLCHAIN" = "x" && TOOLCHAIN='limine'

AC_ARG_VAR(LIMINE_CC, [C compiler command for Limine])

AC_DEFUN([CC_ERROR_MSG], [
    AC_MSG_ERROR([no suitable LIMINE_CC found, run $srcdir/make_toolchain.sh or install x86_64 GCC/Clang])
])

if ! test "x$LIMINE_CC" = "x"; then
    AC_CHECK_PROG([LIMINE_CC_0], [$LIMINE_CC], [yes])
    if ! test "x$LIMINE_CC_0" = "xyes"; then
        AC_MSG_ERROR([LIMINE_CC ($LIMINE_CC) not found])
    fi
else
    if test "$TOOLCHAIN" = 'llvm'; then
        LIMINE_CC='clang'
    else
        LIMINE_CC="$TOOLCHAIN-gcc"
    fi
    AC_CHECK_PROG([LIMINE_CC_1], [$LIMINE_CC], [yes])
    if ! test "x$LIMINE_CC_1" = "xyes"; then
        LIMINE_CC="$TOOLCHAIN-cc"
        AC_CHECK_PROG([LIMINE_CC_2], [$LIMINE_CC], [yes])
        if ! test "x$LIMINE_CC_2" = "xyes"; then
            LIMINE_CC="gcc"
            AC_CHECK_PROG([LIMINE_CC_3], [$LIMINE_CC], [yes])
            if ! test "x$LIMINE_CC_3" = "xyes"; then
                LIMINE_CC="clang"
                AC_CHECK_PROG([LIMINE_CC_4], [$LIMINE_CC], [yes])
                if ! test "x$LIMINE_CC_4" = "xyes"; then
                    LIMINE_CC="cc"
                    AC_CHECK_PROG([LIMINE_CC_5], [$LIMINE_CC], [yes])
                    if ! test "x$LIMINE_CC_5" = "xyes"; then
                        CC_ERROR_MSG
                    fi
                fi
            fi
        fi
    fi
fi

if ! $LIMINE_CC --version >/dev/null 2>&1; then
    CC_ERROR_MSG
fi

if $LIMINE_CC --version | $GREP clang >/dev/null 2>&1; then
    AC_SUBST(USING_CLANG, 1)
elif $LIMINE_CC --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
    AC_SUBST(USING_CLANG, 0)
else
    CC_ERROR_MSG
fi

if ! $LIMINE_CC -dumpmachine >/dev/null 2>&1; then
    CC_ERROR_MSG
fi

CC_MACHINE="$($LIMINE_CC -dumpmachine | dd bs=6 count=1 2>/dev/null)"
if ! test "x$CC_MACHINE" = "xx86_64"; then
    if ! test "x$CC_MACHINE" = "xamd64-"; then
        CC_ERROR_MSG
    fi
fi

AC_DEFUN([GET_BINUTILS_PROG], [
    AC_ARG_VAR(LIMINE_$1, [$3 command for Limine])

    if ! test "x$LIMINE_$1" = "x"; then
        AC_CHECK_PROG([LIMINE_$1_0], [$LIMINE_$1], [yes])
        if ! test "x$LIMINE_$1_0" = "xyes"; then
            AC_MSG_ERROR([LIMINE_$1 ($LIMINE_$1) not found])
        fi
    else
        if test "$TOOLCHAIN" = 'llvm' && test '$2' = 'ld'; then
            LIMINE_$1='ld.lld'
        else
            LIMINE_$1="$TOOLCHAIN-$2"
        fi
        AC_CHECK_PROG([LIMINE_$1_1], [$LIMINE_$1], [yes])
        if ! test "x$LIMINE_$1_1" = "xyes"; then
            LIMINE_$1='g$2'
            AC_CHECK_PROG([LIMINE_$1_2], [$LIMINE_$1], [yes])
            if ! test "x$LIMINE_$1_2" = "xyes"; then
                LIMINE_$1='$2'
                AC_CHECK_PROG([LIMINE_$1_3], [$LIMINE_$1], [yes])
                if ! test "x$LIMINE_$1_3" = "xyes"; then
                    if test '$2' = 'ld'; then
                        LIMINE_$1='ld.lld'
                    else
                        LIMINE_$1='llvm-$2'
                    fi
                    AC_CHECK_PROG([LIMINE_$1_4], [$LIMINE_$1], [yes])
                    if ! test "x$LIMINE_$1_4" = "xyes"; then
                        AC_MSG_ERROR([no suitable LIMINE_$1 found, run $srcdir/make_toolchain.sh or install LLVM/GNU binutils])
                    fi
                fi
            fi
        fi
    fi
])

GET_BINUTILS_PROG(LD, ld, linker)
GET_BINUTILS_PROG(AR, ar, archiver)
GET_BINUTILS_PROG(AS, as, assembler)
GET_BINUTILS_PROG(OBJCOPY, objcopy, objcopy)
GET_BINUTILS_PROG(OBJDUMP, objdump, objdump)
GET_BINUTILS_PROG(READELF, readelf, readelf)

AC_ARG_VAR(LIMINE_CFLAGS, [C flags for Limine [default: -O3 -pipe -Wall -Wextra]])
test "x$LIMINE_CFLAGS" = "x" && LIMINE_CFLAGS='-O3 -pipe -Wall -Wextra'

AC_ARG_VAR(LIMINE_LDFLAGS, [Linker flags for Limine [default: ]])
test "x$LIMINE_LDFLAGS" = "x" && LIMINE_LDFLAGS=' '

LIMINE_COPYRIGHT=$($GREP Copyright "$SRCDIR/LICENSE.md")
AC_SUBST(LIMINE_COPYRIGHT, [$LIMINE_COPYRIGHT])

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile config.h)
AC_OUTPUT
