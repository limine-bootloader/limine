AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine])

AC_PREREQ(2.69)

AC_CONFIG_AUX_DIR([build-aux])

TOOLCHAIN_DIR="$(pwd -P)/toolchain/bin"
PATH="$TOOLCHAIN_DIR$PATH_SEPARATOR/usr/local/bin$PATH_SEPARATOR$PATH"
export PATH

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe -Wall -Wextra'
AC_PROG_CC

werror_state="no"
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [treat warnings as errors]),
    werror_state="$enableval")

if test "$werror_state" = "yes"; then
    AC_SUBST(werror, [-Werror])
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

BUILD_ELTORITO_EFI="default"

AC_ARG_ENABLE([eltorito-efi],
    AS_HELP_STRING([--disable-eltorito-efi], [disable limine-eltorito-efi.bin build]),
    BUILD_ELTORITO_EFI="$enableval")

if ! test "x$BUILD_ELTORITO_EFI" = "xno"; then
    AC_CHECK_PROG([MTOOLS_FOUND], [mcopy], [yes])
    if ! test "x$MTOOLS_FOUND" = "xyes"; then
        if test "x$BUILD_ELTORITO_EFI" = "xyes"; then
            AC_MSG_ERROR([mtools not found, install mtools to build limine-eltorito-efi.bin])
        fi
        AC_MSG_WARN([mtools not found, install mtools to build limine-eltorito-efi.bin])
        BUILD_ELTORITO_EFI="no"
    fi
fi

if ! test "x$BUILD_ELTORITO_EFI" = "xno"; then
    AC_SUBST(build_eltorito_efi, [yes])
fi

AC_ARG_VAR(HOST_CC, [C compiler for the build host [default: $CC]])
test "x$HOST_CC" = "x" && HOST_CC="$CC"

AC_ARG_VAR(TOOLCHAIN, [Alternative toolchain prefix [default: limine]])
test "x$TOOLCHAIN" = "x" && TOOLCHAIN='limine'

AC_ARG_VAR(LIMINE_CC, [C compiler command for Limine [default: $TOOLCHAIN-gcc]])

AC_DEFUN([CC_ERROR_MSG], [
    AC_MSG_ERROR([no suitable LIMINE_CC found, run $srcdir/make_toolchain.sh or install x86_64 GCC/Clang])
])

if ! test "x$LIMINE_CC" = "x"; then
    AC_CHECK_PROG([LIMINE_CC_0], [$LIMINE_CC], [yes])
    if ! test "x$LIMINE_CC_0" = "xyes"; then
        AC_MSG_ERROR([LIMINE_CC ($LIMINE_CC) not found])
    fi
else
    LIMINE_CC="$TOOLCHAIN-gcc"
    AC_CHECK_PROG([LIMINE_CC_1], [$LIMINE_CC], [yes])
    if ! test "x$LIMINE_CC_1" = "xyes"; then
        LIMINE_CC="$CC"
        AC_CHECK_PROG([LIMINE_CC_2], [$LIMINE_CC], [yes])
        if ! test "x$LIMINE_CC_2" = "xyes"; then
            CC_ERROR_MSG
        fi
    fi
fi

if ! $LIMINE_CC --version >/dev/null 2>&1; then
    CC_ERROR_MSG
fi

if $LIMINE_CC --version | $GREP clang >/dev/null 2>&1; then
    AC_SUBST(using_clang, 1)
elif $LIMINE_CC --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
    AC_SUBST(using_clang, 0)
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
    AC_ARG_VAR(LIMINE_$1, [$3 command for Limine [default: $TOOLCHAIN-$2]])

    if ! test "x$LIMINE_$1" = "x"; then
        AC_CHECK_PROG([LIMINE_$1_0], [$LIMINE_$1], [yes])
        if ! test "x$LIMINE_$1_0" = "xyes" || ! $LIMINE_$1 --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
            AC_MSG_ERROR([LIMINE_$1 ($LIMINE_$1) is not a suitable $3])
        fi
    else
        LIMINE_$1="$TOOLCHAIN-$2"
        AC_CHECK_PROG([LIMINE_$1_1], [$LIMINE_$1], [yes])
        if ! test "x$LIMINE_$1_1" = "xyes" || ! $LIMINE_$1 --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
            LIMINE_$1='g$2'
            AC_CHECK_PROG([LIMINE_$1_2], [$LIMINE_$1], [yes])
            if ! test "x$LIMINE_$1_2" = "xyes" || ! $LIMINE_$1 --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
                LIMINE_$1='$2'
                AC_CHECK_PROG([LIMINE_$1_3], [$LIMINE_$1], [yes])
                if ! test "x$LIMINE_$1_3" = "xyes" || ! $LIMINE_$1 --version | $GREP 'Free Software Foundation' >/dev/null 2>&1; then
                    AC_MSG_ERROR([no suitable LIMINE_$1 found, run $srcdir/make_toolchain.sh or install GNU binutils])
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

AC_SUBST(limine_version, m4_esyscmd([./version.sh]))

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
