dnl ***************************************************************************
dnl
dnl Project:  GDAL
dnl Purpose:  Test for SFCGAL library presence
dnl Author:   Avyav Kumar Singh, avyavkumar@gmail.com
dnl	          Ideas borrowed from the old GDAL test and from the m4 file
dnl           gdal/m4/geos.m4 written by Andrey Kiselev for GEOS originally
dnl
dnl ***************************************************************************
dnl Copyright (c) 2016, Avyav Kumar Singh
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
dnl SFCGAL_INIT (MINIMUM_VERSION)
dnl
dnl Test for SFCGAL: define HAVE_SFCGAL, SFCGAL_LIBS, SFCGAL_CFLAGS, SFCGAL_VERSION
dnl
dnl Call as SFCGAL_INIT or SFCGAL_INIT(minimum version) in configure.ac. Test
dnl HAVE_SFCGAL (yes|no) afterwards. If yes, all other vars above can be
dnl used in program.
dnl

AC_DEFUN([SFCGAL_INIT],[
  AC_SUBST(SFCGAL_LIBS)
  AC_SUBST(SFCGAL_CFLAGS)
  AC_SUBST(HAVE_SFCGAL)
  AC_SUBST(SFCGAL_VERSION)

  AC_ARG_WITH(sfcgal,
    AS_HELP_STRING([--with-sfcgal[=ARG]],
                   [Include SFCGAL support (ARG=yes, no or sfcgal-config path)]),,)

  ac_sfcgal_config_auto=no

  if test x"$with_sfcgal" = x"no" ; then

    AC_MSG_RESULT([SFCGAL support disabled])
    SFCGAL_CONFIG=no

  elif test x"$with_sfcgal" = x"yes" -o x"$with_sfcgal" = x"" ; then

    AC_PATH_PROG(SFCGAL_CONFIG, sfcgal-config, no)
    if test x"$with_sfcgal" = x"" ; then
      ac_sfcgal_config_auto=yes
    fi

  else

   ac_sfcgal_config=`basename "$with_sfcgal"`
   ac_sfcgal_config_dir=`AS_DIRNAME(["$with_sfcgal"])`

   AC_CHECK_PROG(
        SFCGAL_CONFIG,
        "$ac_sfcgal_config",
        $with_sfcgal,
        [no],
        ["$ac_sfcgal_config_dir"],
        []
   )

  fi

  if test x"$SFCGAL_CONFIG" != x"no" ; then

    min_sfcgal_version=ifelse([$1], ,1.2.2,$1)

    AC_MSG_CHECKING(for SFCGAL version >= $min_sfcgal_version)

    sfcgal_major_version=`$SFCGAL_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    sfcgal_minor_version=`$SFCGAL_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    sfcgal_micro_version=`$SFCGAL_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

    req_major=`echo $min_sfcgal_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    req_minor=`echo $min_sfcgal_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    req_micro=`echo $min_sfcgal_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

    version_ok="no"
    ac_req_version=`expr $req_major \* 100000 \+  $req_minor \* 100 \+ $req_micro`
    ac_sfcgal_version=`expr $sfcgal_major_version \* 100000 \+  $sfcgal_minor_version \* 100 \+ $sfcgal_micro_version`

    if test $ac_req_version -le $ac_sfcgal_version; then
        version_ok="yes"
        AC_MSG_RESULT([yes])
    fi

    if test $version_ok = "no"; then

      HAVE_SFCGAL="no"
      AC_MSG_RESULT(no)

      if test $ac_sfcgal_config_auto = "yes" ; then
        AC_MSG_WARN([SFCGAL was found on your system, but sfcgal-config reports version ${sfcgal_major_version}.${sfcgal_minor_version}.${sfcgal_micro_version}, need at least $min_sfcgal_version. SFCGAL support disabled.])
      else
        AC_MSG_ERROR([sfcgal-config reports version ${sfcgal_major_version}.${sfcgal_minor_version}.${sfcgal_micro_version}, need at least $min_sfcgal_version or configure --without-sfcgal])
      fi

    else

      HAVE_SFCGAL="no"

      SFCGAL_LIBS="`${SFCGAL_CONFIG} --libs`"
      SFCGAL_CFLAGS="`${SFCGAL_CONFIG} --cflags`"
      SFCGAL_VERSION="`${SFCGAL_CONFIG} --version`"

      ax_save_LIBS="${LIBS}"
      LIBS=${SFCGAL_LIBS}
      ax_save_CFLAGS="${CFLAGS}"
      CFLAGS="${SFCGAL_CFLAGS}"
      ax_save_LDFLAGS="${LDFLAGS}"
      LDFLAGS=""

      AC_CHECK_LIB([SFCGAL],
        [sfcgal_version],
        [HAVE_SFCGAL="yes"],
        [HAVE_SFCGAL="no"],
        []
      )


      if test x"$HAVE_SFCGAL" = "xno"; then
        if test $ac_sfcgal_config_auto = "yes" ; then
          AC_MSG_WARN([SFCGAL was found on your system, but the library could not be linked. SFCGAL support disabled.])
        else
          AC_MSG_ERROR([SFCGAL library could not be linked])
        fi

        SFCGAL_CFLAGS=""
      fi

      CFLAGS="${ax_save_CFLAGS}"
      LIBS="${ax_save_LIBS}"
      LDFLAGS="${ax_save_LDFLAGS}"

    fi

  else

    if test $ac_sfcgal_config_auto = "no" ; then
      AC_MSG_ERROR([SFCGAL support explicitly enabled, but sfcgal-config could not be found])
    fi

  fi
])
