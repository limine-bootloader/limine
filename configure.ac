AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine])

AC_PREREQ(2.69)

AC_CONFIG_AUX_DIR([build-aux])

TOOLCHAIN_DIR="$(pwd -P)/toolchain/bin"
PATH="$TOOLCHAIN_DIR$PATH_SEPARATOR$PATH"
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

AC_CHECK_PROG([NASM_FOUND], [nasm], [yes])
if ! test "x$NASM_FOUND" = "xyes"; then
    AC_MSG_ERROR([NASM not found, please install NASM before configuring.])
fi

AC_ARG_VAR(HOST_CC, [C compiler for the build host [default: $CC]])
test "x$HOST_CC" = "x" && HOST_CC="$CC"

AC_ARG_VAR(TOOLCHAIN, [Alternative toolchain prefix [default: limine]])
test "x$TOOLCHAIN" = "x" && TOOLCHAIN='limine'

AC_ARG_VAR(LIMINE_CFLAGS, [C flags for Limine [default: -O3 -pipe -Wall -Wextra]])
test "x$LIMINE_CFLAGS" = "x" && LIMINE_CFLAGS='-O3 -pipe -Wall -Wextra'

AC_ARG_VAR(LIMINE_LDFLAGS, [Linker flags for Limine [default: ]])
test "x$LIMINE_LDFLAGS" = "x" && LIMINE_LDFLAGS=' '

AC_ARG_VAR(LIMINE_CC, [C compiler command for Limine [default: $TOOLCHAIN-gcc]])
test "x$LIMINE_CC" = "x" && LIMINE_CC="$TOOLCHAIN-gcc"

AC_PATH_PROG([LIMINE_CC_1], [$LIMINE_CC], [no])
if test "x$LIMINE_CC_1" = "xno"; then
    LIMINE_CC="$CC"
    AC_PATH_PROG([LIMINE_CC_2], [$LIMINE_CC], [no])
    if test "x$LIMINE_CC_2" = "xno"; then
        AC_MSG_ERROR([No suitable LIMINE_CC found, run $srcdir/make_toolchain.sh or install GCC/Clang.])
    fi
fi

CC_MACHINE="$($LIMINE_CC -dumpmachine | dd bs=6 count=1 2>/dev/null)"
if ! test "x$CC_MACHINE" = "xx86_64"; then
    if ! test "x$CC_MACHINE" = "xamd64-"; then
        AC_MSG_ERROR([No suitable x86_64 C compiler found, please install x86_64 capable GCC/clang or run $srcdir/make_toolchain.sh])
    fi
fi

AC_DEFUN([GET_TOOLCHAIN_PROG], [
    AC_ARG_VAR(LIMINE_$1, [$3 command for Limine [$TOOLCHAIN-$2]])
    test "x$LIMINE_$1" = "x" && LIMINE_$1="$TOOLCHAIN-$2"

    AC_PATH_PROG([LIMINE_$1_1], [$LIMINE_$1], [no])
    if test "x$LIMINE_$1_1" = "xno"; then
        LIMINE_$1='g$2'
        AC_PATH_PROG([LIMINE_$1_2], [$LIMINE_$1], [no])
        if test "x$LIMINE_$1_2" = "xno"; then
            LIMINE_$1='$2'
            AC_PATH_PROG([LIMINE_$1_3], [$LIMINE_$1], [no], [/usr/local/bin])
            if test "x$LIMINE_$1_3" = "xno"; then
                AC_PATH_PROG([LIMINE_$1_4], [$LIMINE_$1], [no])
                if test "x$LIMINE_$1_4" = "xno"; then
                    AC_MSG_ERROR([No suitable LIMINE_$1 found, run $srcdir/make_toolchain.sh or install GNU binutils.])
                fi
            fi
        fi
    fi

    if ! $LIMINE_$1 --version | grep 'GNU Binutils' >/dev/null; then
        AC_MSG_ERROR([LIMINE_$1 ($LIMINE_$1) is not from GNU binutils. Run $srcdir/make_toolchain.sh or install GNU binutils.])
    fi
])

GET_TOOLCHAIN_PROG(LD, ld, Linker)
GET_TOOLCHAIN_PROG(AR, ar, Archiver)
GET_TOOLCHAIN_PROG(AS, as, Assembler)
GET_TOOLCHAIN_PROG(OBJCOPY, objcopy, Objcopy)
GET_TOOLCHAIN_PROG(OBJDUMP, objdump, Objdump)
GET_TOOLCHAIN_PROG(READELF, readelf, Readelf)

AC_SUBST(limine_version, m4_esyscmd([./version.sh]))

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
