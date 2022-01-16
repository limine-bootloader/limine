AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine])

AC_PREREQ(2.69)

AC_SUBST(limine_version, m4_esyscmd([./version.sh]))

AC_PROG_CC

AC_ARG_VAR(TOOLCHAIN, [Alternative toolchain prefix [limine]])
test "x$TOOLCHAIN" = "x" && TOOLCHAIN='limine'

AC_ARG_VAR(LIMINE_CFLAGS, [C flags for Limine [-O3 -pipe -Wall -Wextra]])
test "x$LIMINE_CFLAGS" = "x" && LIMINE_CFLAGS='-O3 -pipe -Wall -Wextra'

AC_ARG_VAR(LIMINE_LDFLAGS, [Linker flags for Limine [ ]])
test "x$LIMINE_LDFLAGS" = "x" && LIMINE_LDFLAGS=' '

AC_ARG_VAR(LIMINE_CC, [C compiler command for Limine [$(TOOLCHAIN)-gcc]])
test "x$LIMINE_CC" = "x" && LIMINE_CC='$(TOOLCHAIN)-gcc'

AC_ARG_VAR(LIMINE_LD, [Linker command for Limine [$(TOOLCHAIN)-ld]])
test "x$LIMINE_LD" = "x" && LIMINE_LD='$(TOOLCHAIN)-ld'

AC_ARG_VAR(LIMINE_AR, [Archiver command for Limine [$(TOOLCHAIN)-ar]])
test "x$LIMINE_AR" = "x" && LIMINE_AR='$(TOOLCHAIN)-ar'

AC_ARG_VAR(LIMINE_AS, [Assembler command for Limine [$(TOOLCHAIN)-as]])
test "x$LIMINE_AS" = "x" && LIMINE_AS='$(TOOLCHAIN)-as'

AC_ARG_VAR(LIMINE_OBJCOPY, [Objcopy command for Limine [$(TOOLCHAIN)-objcopy]])
test "x$LIMINE_OBJCOPY" = "x" && LIMINE_OBJCOPY='$(TOOLCHAIN)-objcopy'

AC_ARG_VAR(LIMINE_OBJDUMP, [Objdump command for Limine [$(TOOLCHAIN)-objdump]])
test "x$LIMINE_OBJDUMP" = "x" && LIMINE_OBJDUMP='$(TOOLCHAIN)-objdump'

AC_ARG_VAR(LIMINE_READELF, [Readelf command for Limine [$(TOOLCHAIN)-readelf]])
test "x$LIMINE_READELF" = "x" && LIMINE_READELF='$(TOOLCHAIN)-readelf'

werror_state="yes"
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--disable-werror], [do not treat warnings as errors]),
    werror_state="$enableval")

if test "$werror_state" = "yes"; then
    AC_SUBST(werror, [-Werror])
fi

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
