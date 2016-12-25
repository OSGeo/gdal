dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([PROJ.4 Projections], 4.9.3, [warmerdam@pobox.com], proj)
AC_CONFIG_MACRO_DIR([m4])
AC_LANG(C)

AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(src/proj_config.h)

dnl Enable as much warnings as possible
AX_CFLAGS_WARN_ALL(C_WFLAGS)
AC_SUBST(C_WFLAGS,$C_WFLAGS)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL

dnl Checks for libraries.
AC_CHECK_LIB(m,exp,,,)

dnl We check for headers
AC_HEADER_STDC

AC_CHECK_FUNC(localeconv, [AC_DEFINE(HAVE_LOCALECONV,1,[Define to 1 if you have localeconv])])

dnl ---------------------------------------------------------------------------
dnl Check for JNI support.
dnl ---------------------------------------------------------------------------

JNI_INCLUDE=
export JNI_INCLUDE

AC_ARG_WITH(jni,
            [AS_HELP_STRING([--with-jni=dir],
                            [Include Java/JNI support, add optional include dir])
            ],,)

if test "$with_jni" = "yes" ; then

  AC_DEFINE(JNI_ENABLED,1,[Enabled for Java/JNI Support])

  AC_MSG_CHECKING(whether to enable Java/JNI support)

  AC_MSG_RESULT([enabled])

elif test "$with_jni" != "no" -a "$with_jni" != "" ; then

  AC_MSG_CHECKING(whether to enable Java/JNI support)

  if test \! -r "$with_jni/jni.h" ; then
    AC_MSG_ERROR(Did not find $with_jni/jni.h)
  fi

  AC_DEFINE(JNI_ENABLED,1,[Enabled for Java/JNI Support])

  AC_MSG_RESULT([enabled])

  JNI_INCLUDE="-I$with_jni"

elif test "$with_jni" = "" ; then
  AC_CHECK_HEADERS(jni.h)

  AC_MSG_CHECKING(whether to enable Java/JNI support)

  if test "$ac_cv_header_jni_h" = "no" ; then
    AC_MSG_RESULT([disabled])
  else
    AC_MSG_RESULT([enabled])
    AC_DEFINE(JNI_ENABLED,1,[Enabled for Java/JNI Support])
  fi

else
  AC_MSG_CHECKING(whether to enable Java/JNI support)

  AC_MSG_RESULT([disabled])
fi

AC_SUBST(JNI_INCLUDE,$JNI_INCLUDE)

dnl ---------------------------------------------------------------------------
dnl Provide a mechanism to disable real mutex support (if lacking win32 or
dnl posix mutexes for instance).
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([mutex],
	    AS_HELP_STRING([--without-mutex],
	       [Disable real mutex locks (lacking pthreads)]),,)

AC_MSG_CHECKING([for mutexes])
THREAD_LIB=""
if test "$with_mutex" = yes -o x"$with_mutex" = x ; then
    MUTEX_SETTING=pthread
    AC_CHECK_LIB(pthread,pthread_create,PTHREAD_EXISTS=YES,,,)
    if test -n "$PTHREAD_EXISTS" ; then
        THREAD_LIB="-lpthread"
    fi
    AC_CHECK_LIB(pthread,pthread_mutexattr_settype,,,)
    AC_CHECK_DECL(PTHREAD_MUTEX_RECURSIVE,
                  AC_DEFINE(HAVE_PTHREAD_MUTEX_RECURSIVE, [], [Define if your pthreads implementation have PTHREAD_MUTEX_RECURSIVE]),
                  ,
                  [#include <pthread.h>])
    AC_MSG_RESULT([enabled, pthread])
else
    MUTEX_SETTING=stub
    AC_MSG_RESULT([disabled by user])
fi

AC_SUBST(MUTEX_SETTING,$MUTEX_SETTING)
AC_SUBST(THREAD_LIB,$THREAD_LIB)

AC_CONFIG_FILES([Makefile cmake/Makefile src/Makefile
                 man/Makefile man/man1/Makefile man/man3/Makefile nad/Makefile
                 jniwrap/Makefile jniwrap/org/Makefile jniwrap/org/proj4/Makefile])
AC_CONFIG_FILES([nad/install], [chmod +x nad/install])

AC_CONFIG_FILES([proj.pc])

AC_OUTPUT
