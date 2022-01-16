AC_INIT([Limine], m4_esyscmd([./version.sh]), [https://github.com/limine-bootloader/limine/issues], [limine])

AC_PREREQ(2.69)

AC_PROG_CC

AC_SUBST(limine_version, m4_esyscmd([./version.sh]))

AC_ARG_VAR(CC, [C compiler command [cc]])
test -z "$CC" && CC='cc'

AC_ARG_VAR(TOOLCHAIN, [Alternative toolchain prefix [limine]])
test -z "$TOOLCHAIN" && TOOLCHAIN='limine'

AC_ARG_VAR(TOOLCHAIN_CC, [Toolchain C compiler command [$(TOOLCHAIN)-gcc]])
test -z "$TOOLCHAIN_CC" && TOOLCHAIN_CC='$(TOOLCHAIN)-gcc'

AC_ARG_VAR(TOOLCHAIN_LD, [Toolchain linker command [$(TOOLCHAIN)-ld]])
test -z "$TOOLCHAIN_LD" && TOOLCHAIN_LD='$(TOOLCHAIN)-ld'

AC_ARG_VAR(TOOLCHAIN_AR, [Toolchain archiver command [$(TOOLCHAIN)-ar]])
test -z "$TOOLCHAIN_AR" && TOOLCHAIN_AR='$(TOOLCHAIN)-ar'

AC_ARG_VAR(TOOLCHAIN_OBJCOPY, [Toolchain objcopy command [$(TOOLCHAIN)-objcopy]])
test -z "$TOOLCHAIN_OBJCOPY" && TOOLCHAIN_OBJCOPY='$(TOOLCHAIN)-objcopy'

AC_ARG_VAR(TOOLCHAIN_OBJDUMP, [Toolchain objdump command [$(TOOLCHAIN)-objdump]])
test -z "$TOOLCHAIN_OBJDUMP" && TOOLCHAIN_OBJDUMP='$(TOOLCHAIN)-objdump'

AC_ARG_VAR(TOOLCHAIN_READELF, [Toolchain readelf command [$(TOOLCHAIN)-readelf]])
test -z "$TOOLCHAIN_READELF" && TOOLCHAIN_READELF='$(TOOLCHAIN)-readelf'

AC_PREFIX_DEFAULT(/usr/local)

AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
