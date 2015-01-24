#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dchat], [0.2], [christoph.mahrl@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/dchat.c])
AC_CONFIG_HEADERS([config.h])

AC_DEFINE([CONFIG_PATH], "/usr/local/etc/dchat/dchat.conf", [Path to configuration diretory])

PREFIX=`test "$prefix" = NONE && prefix=""; eval echo "${prefix}"`

AC_DEFINE_UNQUOTED([INP_SOCK_PATH], ["$PREFIX/var/run/dinp.sock"], [Location of user interface input socket])
AC_DEFINE_UNQUOTED([OUT_SOCK_PATH], ["$PREFIX/var/run/dout.sock"], [Location of user interface output socket])
AC_DEFINE_UNQUOTED([LOG_SOCK_PATH], ["$PREFIX/var/run/dlog.sock"], [Location of user interface logging socket])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC

#Doxygen
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([dchat], [doxygen.cfg])

# Checks for libraries.
AC_CHECK_LIB([readline], [readline])
AX_PTHREAD([LIBS+="$PTHREAD_CFLAGS $PTHREAD_LIBS"])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h syslog.h unistd.h getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset select socket strchr strerror strtol getopt_long])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile])

AC_OUTPUT
