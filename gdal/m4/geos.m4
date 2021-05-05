dnl ***************************************************************************
dnl $Id$
dnl
dnl Project:  GDAL
dnl Purpose:  Test for GEOS library presence
dnl Author:   Andrey Kiselev, dron@ak4719.spb.edu
dnl	      Ideas borrowed from the old GDAL test and from the macro
dnl           supplied with GEOS package.
dnl
dnl ***************************************************************************
dnl Copyright (c) 2006, Andrey Kiselev
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, including without limitation
dnl the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl and/or sell copies of the Software, and to permit persons to whom the
dnl Software is furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included
dnl in all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
dnl THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl DEALINGS IN THE SOFTWARE.
dnl ***************************************************************************

dnl
dnl GEOS_INIT (MINIMUM_VERSION)
dnl
dnl Test for GEOS: define HAVE_GEOS, GEOS_LIBS, GEOS_CFLAGS, GEOS_VERSION
dnl
dnl Call as GEOS_INIT or GEOS_INIT(minimum version) in configure.ac. Test
dnl HAVE_GEOS (yes|no) afterwards. If yes, all other vars above can be
dnl used in program.
dnl

AC_DEFUN([GEOS_INIT],[
  AC_SUBST(GEOS_LIBS)
  AC_SUBST(GEOS_CFLAGS)
  AC_SUBST(HAVE_GEOS)
  AC_SUBST(GEOS_VERSION)

  AC_ARG_WITH(geos,
    AS_HELP_STRING([--with-geos[=ARG]],
                   [Include GEOS support (ARG=yes, no or geos-config path)]),,)

  ac_geos_config_auto=no

  if test x"$with_geos" = x"no" ; then

    AC_MSG_RESULT([GEOS support disabled])
    GEOS_CONFIG=no
    HAVE_GEOS=no

  elif test x"$with_geos" = x"yes" -o x"$with_geos" = x"" ; then

    AC_PATH_PROG(GEOS_CONFIG, geos-config, no)
    if test x"$with_geos" = x"" ; then
      ac_geos_config_auto=yes
    fi

  else

   ac_geos_config=`basename "$with_geos"`
   ac_geos_config_dir=`AS_DIRNAME(["$with_geos"])`

   AC_CHECK_PROG(
        GEOS_CONFIG,
        "$ac_geos_config",
        $with_geos,
        [no],
        ["$ac_geos_config_dir"],
        []
   )

  fi

  if test x"$GEOS_CONFIG" != x"no" ; then

    min_geos_version=ifelse([$1], ,1.0.0,$1)

    AC_MSG_CHECKING(for GEOS version >= $min_geos_version)

    geos_major_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    geos_minor_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    geos_micro_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

    req_major=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    req_minor=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    req_micro=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

    version_ok="no"
    ac_req_version=`expr $req_major \* 100000 \+  $req_minor \* 100 \+ $req_micro`
    ac_geos_version=`expr $geos_major_version \* 100000 \+  $geos_minor_version \* 100 \+ $geos_micro_version`

    if test $ac_req_version -le $ac_geos_version; then
        version_ok="yes"
        AC_MSG_RESULT([yes])
    fi

    if test $version_ok = "no"; then

      HAVE_GEOS="no"
      AC_MSG_RESULT(no)

      if test $ac_geos_config_auto = "yes" ; then
        AC_MSG_WARN([GEOS was found on your system, but geos-config reports version ${geos_major_version}.${geos_minor_version}.${geos_micro_version}, need at least $min_geos_version. GEOS support disabled.])
      else
        AC_MSG_ERROR([geos-config reports version ${geos_major_version}.${geos_minor_version}.${geos_micro_version}, need at least $min_geos_version or configure --without-geos])
      fi

    else

      HAVE_GEOS="no"

      GEOS_LIBS="`${GEOS_CONFIG} --ldflags` -lgeos_c"
      GEOS_CFLAGS="`${GEOS_CONFIG} --cflags`"
      GEOS_VERSION="`${GEOS_CONFIG} --version`"

      ax_save_LIBS="${LIBS}"
      LIBS=${GEOS_LIBS}
      ax_save_CFLAGS="${CFLAGS}"
      CFLAGS="${GEOS_CFLAGS}"
      ax_save_LDFLAGS="${LDFLAGS}"
      LDFLAGS=""

      AC_CHECK_LIB([geos_c],
        [GEOSversion],
        [HAVE_GEOS="yes"],
        [HAVE_GEOS="no"],
        []
      )

      if test x"$HAVE_GEOS" = "xno"; then
        if test $ac_geos_config_auto = "yes" ; then
          AC_MSG_WARN([GEOS was found on your system, but the library could not be linked. GEOS support disabled.])
        else
          AC_MSG_ERROR([GEOS library could not be linked])
        fi

        GEOS_CFLAGS=""

      fi

      CFLAGS="${ax_save_CFLAGS}"
      LIBS="${ax_save_LIBS}"
      LDFLAGS="${ax_save_LDFLAGS}"

    fi

  else

    if test x"$with_geos" != x"no" -a x"$with_geos" != x ; then
      AC_MSG_ERROR([GEOS support explicitly enabled, but geos-config could not be found])
    fi

  fi
])

