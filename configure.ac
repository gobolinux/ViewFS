
AC_PREREQ(2.59)
AC_INIT([viewfs], [0.1], [hhmuhamm@us.ibm.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/viewfs.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([fuse], [fuse_get_context])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h stddef.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strchr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
