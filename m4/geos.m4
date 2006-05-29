dnl
dnl GEOS_INIT (MINIMUM_VERSION)
dnl
dnl Test for GEOS: define HAVE_GEOS, GEOS_LIBS, GEOS_CFLAGS, GEOS_VERSION
dnl 
dnl Call as GEOS_INIT or GEOS_INIT(minimum version) in configure.in. Test
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

  if test x"$with_geos" = x"no" ; then

    AC_MSG_RESULT([GEOS support disabled])
    GEOS_CONFIG=no

  elif test x"$with_geos" = x"yes" -o x"$with_geos" = x"" ; then

    AC_PATH_PROG(GEOS_CONFIG, geos-config, no)

  else

   if test "`basename xx/$with_geos`" = "geos-config" ; then
      AC_MSG_NOTICE([GEOS enabled with provided geos-config])
      GEOS_CONFIG="$with_geos"
    else
      AC_MSG_ERROR([--with-geos should have yes, no or a path to geos-config])
    fi

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
    if test $req_major -le $geos_major_version; then
       if test $req_minor -le $geos_minor_version; then
          if test $req_micro -le $geos_micro_version; then
             version_ok="yes"
          fi
       fi
    fi

    if test $version_ok = "no"; then
       HAVE_GEOS="no"
       AC_MSG_RESULT(no)
       AC_MSG_ERROR([geos-config reports version ${geos_major_version}.${geos_minor_version}.${geos_micro_version}, need at least $min_geos_version or configure --without-geos])
    else
      
      AC_MSG_RESULT(yes)
      HAVE_GEOS="yes"
      GEOS_LIBS="`${GEOS_CONFIG} --libs`"
      GEOS_CFLAGS="`${GEOS_CONFIG} --cflags`"
      GEOS_VERSION="`${GEOS_CONFIG} --version`"

      ax_save_LIBS="${LIBS}"
      LIBS=${GEOS_LIBS}
      AC_CHECK_LIB(geos, main, HAVE_GEOS=yes, HAVE_GEOS=no,)
      LIBS=${ax_save_LIBS}

    fi

  fi
])

