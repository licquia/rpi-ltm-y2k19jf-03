#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ltmy2kd], [1.0])
AC_CONFIG_SRCDIR([include/ltmy2k19jf03.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [AC_SUBST(PTHREAD_LIB, -lpthread)])
AC_CHECK_LIB([wiringPi], [wiringPiSetupGpio])
AC_SUBST(HAVE_LIBWIRINGPI, $ac_cv_lib_wiringPi_wiringPiSetupGpio)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([gettimeofday mkfifo strerror strtol])

AC_CONFIG_FILES([Makefile etc/ltmy2kd.init])
AC_OUTPUT
