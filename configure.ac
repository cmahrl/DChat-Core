#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dchat], [0.1], [christoph.mahrl@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/dchat.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([readline], [readline])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset select socket strchr strerror strtol])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile])
AC_OUTPUT
