dnl ***************************************************************************
dnl $Id$
dnl
dnl Project:  GDAL
dnl Purpose:  Configure extra local definitions.
dnl Author:   Frank Warmerdam, warmerdam@pobox.com
dnl
dnl ***************************************************************************
dnl Copyright (c) 2000, Frank Warmerdam
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

dnl ---------------------------------------------------------------------------
dnl Try to establish a 64 bit integer type.
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_HAVE_LONG_LONG],
[
  AC_MSG_CHECKING([for 64bit integer type])

  echo 'int main() { return (int)(long long)(0); }' >> conftest.c
  if test -z "`${CC} ${CFLAGS} -o conftest conftest.c 2>&1`" ; then
    AC_DEFINE(HAVE_LONG_LONG, 1, [Define to 1, if your compiler supports long long data type])
    AC_MSG_RESULT([long long])
  else
    AC_MSG_ERROR([long long not found])
  fi
  rm -rf conftest*
])

# AC_LANG_FUNC_LINK_TRY_CUSTOM(C++)(FUNCTION,INCLUDE,CODE)
# ----------------------------------
m4_define([AC_LANG_FUNC_LINK_TRY_CUSTOM],
[AC_LANG_PROGRAM(
[
#include <assert.h>
$2
void test_f()
{
  $3
}
])])

# -----------------------------------------------------------------
# AC_CHECK_FUNC_CUSTOM(FUNCTION, [INCLUDE], [CODE],
#                      [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -----------------------------------------------------------------
# This function is primarily added to facilitate testing that
# function prototypes are properly found such that functions can
# be compiled properly in C++.  In particular, we want to include
# the real include file, not internal define prototypes.
#
# e.g.
# AC_LANG_PUSH(C++)
# AC_CHECK_FUNC_CUSTOM(gmtime_r,[#include <time.h>],[time_t t; struct tm ltime; t = time(0); gmtime_r( &t, &ltime );])
# AC_LANG_POP(C++)
# -----------------------------------------------------------------
AC_DEFUN([AC_CHECK_FUNC_CUSTOM],
[AS_VAR_PUSHDEF([ac_var], [ac_cv_func_$1])dnl
AC_CACHE_CHECK([for $1], ac_var,
[AC_LINK_IFELSE([AC_LANG_FUNC_LINK_TRY_CUSTOM([$1],[$2],[$3])],
                [AS_VAR_SET(ac_var, yes)],
                [AS_VAR_SET(ac_var, no)])])
AS_IF([test AS_VAR_GET(ac_var) = yes], [$4], [$5])dnl
AS_VAR_POPDEF([ac_var])dnl
dnl AC_MSG_RESULT([AC_LANG_FUNC_LINK_TRY_CUSTOM([$1],[$2],[$3])])
dnl exit
])# AC_CHECK_FUNC


dnl ---------------------------------------------------------------------------
dnl Check for Unix 64 bit STDIO API (fseek64, ftell64 like on IRIX).
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_UNIX_STDIO_64],
[
  AC_ARG_WITH(unix-stdio-64,[  --with-unix-stdio-64[=ARG] Utilize 64 stdio api (yes/no)],,)

  AC_MSG_CHECKING([for 64bit file io])

  HAVE_UNIX_STDIO_64="$with_unix_stdio_64"
  HAVE_MINGW_64_IO=no

  dnl Special case when using mingw cross compiler.
  dnl /* We need __MSVCRT_VERSION__ >= 0x0601 to have "struct __stat64" */
  dnl /* Latest versions of mingw32 define it, but with older ones, */
  dnl /* we need to define it manually */
  if test x"$with_unix_stdio_64" = x"" ; then
    echo '#if defined(__MINGW32__)' > conftest.c
    echo '#ifndef __MSVCRT_VERSION__' >> conftest.c
    echo '#define __MSVCRT_VERSION__ 0x0601' >> conftest.c
    echo '#endif' >> conftest.c
    echo '#endif' >> conftest.c
    echo '#include <sys/types.h>' >> conftest.c
    echo '#include <sys/stat.h>' >> conftest.c
    echo 'int main() { struct __stat64 buf; _stat64( "", &buf ); return 0; }' >> conftest.c
    if test -z "`${CC} ${CFLAGS} -o conftest conftest.c 2>&1`" ; then
        HAVE_UNIX_STDIO_64=no
        HAVE_MINGW_64_IO=yes
        AC_DEFINE_UNQUOTED(VSI_STAT64,_stat64, [Define to name of 64bit stat function])
        AC_DEFINE_UNQUOTED(VSI_STAT64_T,__stat64, [Define to name of 64bit stat structure])
    fi
    rm -rf conftest*
  fi

  if test x"$HAVE_UNIX_STDIO_64" = x"yes" ; then
    HAVE_UNIX_STDIO_64=yes
    VSI_FTELL64=ftell64
    VSI_FSEEK64=fseek64
  fi

  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { long long off=0; fseek64(NULL, off, SEEK_SET); off = ftell64(NULL); return (int)off; }' >> conftest.c
    if test -z "`${CC} ${CFLAGS} -o conftest conftest.c 2>&1`" ; then
      HAVE_UNIX_STDIO_64=yes
      VSI_FTELL64=ftell64
      VSI_FSEEK64=fseek64
    fi
    rm -rf conftest*
  fi

  dnl I use CXX in this one, to ensure that the prototypes are available.
  dnl these functions seem to exist on Linux, but aren't normally defined
  dnl by stdio.h.  With CXX (C++) this becomes a fatal error.

  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.cpp
    echo 'int main() { long long off=0; fseeko64(NULL, off, SEEK_SET); off = ftello64(NULL); return (int)off; }' >> conftest.cpp
    if test -z "`${CXX} ${CXXFLAGS} -o conftest conftest.cpp 2>&1`" ; then
      HAVE_UNIX_STDIO_64=yes
      VSI_FTELL64=ftello64
      VSI_FSEEK64=fseeko64
    fi
    rm -rf conftest*
  fi

  dnl This is much like the first test, but we predefine _LARGEFILE64_SOURCE
  dnl before including stdio.h.  This should work on Linux 2.4 series systems.

  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    echo '#define _LARGEFILE64_SOURCE' > conftest.cpp
    echo '#include <stdio.h>' >> conftest.cpp
    echo 'int main() { long long off=0; fseeko64(NULL, off, SEEK_SET); off = ftello64(NULL); return (int)off; }' >> conftest.cpp
    if test -z "`${CXX} ${CXXFLAGS} -o conftest conftest.cpp 2>&1`" ; then
      HAVE_UNIX_STDIO_64=yes
      VSI_FTELL64=ftello64
      VSI_FSEEK64=fseeko64
      AC_DEFINE(VSI_NEED_LARGEFILE64_SOURCE, 1, [Define to 1, if you have LARGEFILE64_SOURCE])
    fi
    rm -rf conftest*
  fi

  dnl Test special MacOS (Darwin) case.

  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    case "${host_os}" in
      darwin*)
        HAVE_UNIX_STDIO_64=yes
        VSI_FTELL64=ftello
        VSI_FSEEK64=fseeko
        ;;
    esac
  fi

  dnl Test for BSD systems that support ftello/fseeko.
  dnl OpenBSD throws warnings about using strcpy/strcat, so we use CC instead of CXX

  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { fpos_t off=0; fseeko(NULL, off, SEEK_SET); off = ftello(NULL); return (int)off; }' >> conftest.c
    if test -z "`${CC} ${CFLAGS} -o conftest conftest.c 2>&1`" ; then
      HAVE_UNIX_STDIO_64=yes
      VSI_FTELL64=ftello
      VSI_FSEEK64=fseeko
    fi
    rm -rf conftest*
  fi

  dnl Needed for netBSD
  if test x"$HAVE_UNIX_STDIO_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { off_t off=0; fseeko(NULL, off, SEEK_SET); off = ftello(NULL); return (int)off; }' >> conftest.c
    if test -z "`${CC} ${CFLAGS} -o conftest conftest.c 2>&1`" ; then
      HAVE_UNIX_STDIO_64=yes
      VSI_FTELL64=ftello
      VSI_FSEEK64=fseeko
    fi
    rm -rf conftest*
  fi

  if test x"$HAVE_UNIX_STDIO_64" = x"yes" ; then
    AC_MSG_RESULT([yes])

    case "${host_os}" in
      darwin*)
        VSI_STAT64=stat
        VSI_STAT64_T=stat
        ;;
      *)
        AC_CHECK_FUNC(stat64, VSI_STAT64=stat64 VSI_STAT64_T=stat64, VSI_STAT64=stat VSI_STAT64_T=stat)
        ;;
    esac
    AC_CHECK_FUNC(fopen64, VSI_FOPEN64=fopen64, VSI_FOPEN64=fopen)
    AC_CHECK_FUNC(ftruncate64, VSI_FTRUNCATE64=ftruncate64, VSI_FTRUNCATE64=ftruncate)

    AC_DEFINE(UNIX_STDIO_64, 1, [Define to 1 if you have fseek64, ftell64])

    export VSI_FTELL64 VSI_FSEEK64 VSI_STAT64 VSI_STAT64_T VSI_OPEN64 VSI_FTRUNCATE64
    AC_DEFINE_UNQUOTED(VSI_FTELL64,$VSI_FTELL64, [Define to name of 64bit ftell func])
    AC_DEFINE_UNQUOTED(VSI_FSEEK64,$VSI_FSEEK64, [Define to name of 64bit fseek func])
    AC_DEFINE_UNQUOTED(VSI_STAT64,$VSI_STAT64, [Define to name of 64bit stat function])
    AC_DEFINE_UNQUOTED(VSI_STAT64_T,$VSI_STAT64_T, [Define to name of 64bit stat structure])
    AC_DEFINE_UNQUOTED(VSI_FOPEN64,$VSI_FOPEN64, [Define to name of 64bit fopen function])
    AC_DEFINE_UNQUOTED(VSI_FTRUNCATE64,$VSI_FTRUNCATE64, [Define to name of 64bit ftruncate function])
  else
    AC_MSG_RESULT([no])
    if test "$HAVE_MINGW_64_IO" = "no"; then
        if test x"$with_unix_stdio_64" = x"no" ; then
            CXXFLAGS="$CXXFLAGS -DBUILD_WITHOUT_64BIT_OFFSET"
            AC_MSG_WARN([64-bit file I/O missing. Build will not support files larger than 4 GB])
        else
            AC_MSG_ERROR([64-bit file I/O missing. Build will not support files larger than 4 GB. If that is intended, ./configure --with-unix-stdio-64=no])
        fi
    fi
  fi

])

AC_DEFUN([AC_COMPILER_LOCALHACK],
[
  AC_MSG_CHECKING([if local/include already standard])

  rm -f comp.out
  echo 'int main() { int i = 1; if( *((unsigned char *) &i) == 0 ) printf( "BIGENDIAN"); return 0; }' >> conftest.c
  ${CC} $CPPFLAGS $EXTRA_INCLUDES -o conftest conftest.c 2> comp.out
  COMP_CHECK=`grep "system directory" comp.out | grep /usr/local/include`
  if test -z "$COMP_CHECK" ; then
     AC_MSG_RESULT([no, everything is ok])
  else
     AC_MSG_RESULT([yes, stripping extras])
     CXXFLAGS=`echo "$CXXFLAGS " | sed "s/-I\/usr\/local\/include //"`
     CFLAGS=`echo "$CFLAGS " | sed "s/-I\/usr\/local\/include //"`
     EXTRA_INCLUDES=`echo "$EXTRA_INCLUDES " | sed "s/-I\/usr\/local\/include //"`
  fi
  rm -f comp.out
])

AC_DEFUN([AC_COMPILER_PIC],
[
	echo 'void f(){}' > conftest.c
	if test -z "`${CC-cc} $CFLAGS -fPIC -c conftest.c 2>&1`"; then
	  CFLAGS="$CFLAGS -fPIC"
	fi
	if test -z "`${CXX-g++} $CXXFLAGS -fPIC -c conftest.c 2>&1`"; then
	  CXXFLAGS="$CXXFLAGS -fPIC"
	fi
	rm -rf conftest*
])

dnl
dnl Look for OGDI, and verify that we can link and run.
dnl
AC_DEFUN([AC_TRY_OGDI],
[
  saved_LIBS="$LIBS"
  OGDI_LIBS=" -logdi -lzlib"
  LIBS="$saved_LIBS $OGDI_LIBS"
  AC_TRY_LINK(,[ void *cln_CreateClient(); cln_CreateClient(); ],
	HAVE_OGDI=yes, HAVE_OGDI=no)

  if test "$HAVE_OGDI" = "no" ; then
    OGDI_LIBS="-L$TOPDIR/bin/$TARGET -logdi -lzlib"
    LIBS="$saved_LIBS $OGDI_LIBS"
    AC_TRY_LINK(,[ void *cln_CreateClient(); cln_CreateClient(); ],
	  HAVE_OGDI=yes, HAVE_OGDI=no)
  fi

  if test "$HAVE_OGDI" = "yes" ; then
    AC_CHECK_HEADER(ecs.h,have_ecs_h=yes,have_ecs_h=no)

    if test "$have_ecs_h" = "no"; then
      if test -f "$TOPDIR/ogdi/include/ecs.h" ; then
        echo "Found ecs.h in $TOPDIR/include"
        OGDI_INCLUDE="-I$TOPDIR/ogdi/include -I$TOPDIR/proj"
      else
        HAVE_OGDI=no
      fi
    fi
  fi

  if test "$HAVE_OGDI" = "no" ; then
    OGDI_LIBS=""
    OGDI_INCLUDE=""
    echo "checking for OGDI ... no"
  else
    echo "checking for OGDI ... yes"
  fi

  AC_SUBST(HAVE_OGDI,$HAVE_OGDI)
  AC_SUBST(OGDI_INCLUDE,$OGDI_INCLUDE)
  AC_SUBST(OGDI_LIBS,$OGDI_LIBS)

  LIBS="$saved_LIBS"
])

dnl
dnl Try to find something to link shared libraries with.  Use "c++ -shared"
dnl in preference to "ld -shared" because it will link in required c++
dnl run time support for us.
dnl
AC_DEFUN([AC_LD_SHARED],
[
  echo 'void g(); int main(){ g(); return 0; }' > conftest1.c

  echo '#include <stdio.h>' > conftest2.c
  echo 'void g(); void g(){printf("");}' >> conftest2.c
  ${CC} ${CFLAGS} -c conftest2.c

  SO_EXT="so"
  export SO_EXT
  LD_SHARED="/bin/true"
  if test ! -z "`uname -a | grep IRIX`" ; then
    IRIX_ALL=-all
  else
    IRIX_ALL=
  fi

  AC_ARG_WITH(ld-shared,[  --without-ld-shared   Disable shared library support],,)

  if test "$with_ld_shared" != "" ; then
    if test "$with_ld_shared" = "no" ; then
      echo "user disabled shared library support."
    elif test "$with_ld_shared" = "yes" ; then
      AC_MSG_ERROR([--with-ld-shared not supported])
    else
      echo "using user supplied .so link command ... $with_ld_shared"
    fi
    LD_SHARED="$with_ld_shared"
  fi

  dnl Check For Cygwin case.  Actually verify that the produced DLL works.

  if test ! -z "`uname -a | grep CYGWIN`" \
        -a "$LD_SHARED" = "/bin/true" \
	-a -z "`gcc -shared conftest2.o -o libconftest.dll`" ; then
    if test -z "`${CC} conftest1.c -L./ -lconftest -o conftest1 2>&1`"; then
      LD_LIBRARY_PATH_OLD="$LD_LIBRARY_PATH"
      if test -z "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="`pwd`"
      else
        LD_LIBRARY_PATH="`pwd`:$LD_LIBRARY_PATH"
      fi
      export LD_LIBRARY_PATH
      if test -z "`./conftest1 2>&1`" ; then
        echo "checking for Cygwin gcc -shared ... yes"
        LD_SHARED="c++ -shared"
        SO_EXT="dll"
      fi
      LD_LIBRARY_PATH="$LD_LIBRARY_PATH_OLD"
    fi
  fi

  dnl Test special MacOS (Darwin) case.

  if test ! -z "`uname | grep Darwin`" \
          -a "$LD_SHARED" = "/bin/true" \
          -a -z "`${CXX} -dynamiclib conftest2.o -o libconftest.so 2>&1`" ; then
    ${CC} -c conftest1.c
    if test -z "`${CXX} conftest1.o libconftest.so -o conftest1 2>&1`"; then
      DYLD_LIBRARY_PATH_OLD="$DYLD_LIBRARY_PATH"
      if test -z "$DYLD_LIBRARY_PATH" ; then
        DYLD_LIBRARY_PATH="`pwd`"
      else
        DYLD_LIBRARY_PATH="`pwd`:$DYLD_LIBRARY_PATH"
      fi
      export DYLD_LIBRARY_PATH
      if test -z "`./conftest1 2>&1`" ; then
        echo "checking for ${CXX} -dynamiclib ... yes"
        LD_SHARED="${CXX} -dynamiclib"
	SO_EXT=dylib
      fi
      DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH_OLD"
    fi
    rm -f conftest1.o
  fi

  if test "$LD_SHARED" = "/bin/true" \
	-a -z "`${CXX} -shared $IRIX_ALL conftest2.o -o libconftest.so 2>&1|grep -v WARNING`" ; then
    if test -z "`${CC} conftest1.c libconftest.so -o conftest1 2>&1`"; then
      LD_LIBRARY_PATH_OLD="$LD_LIBRARY_PATH"
      if test -z "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="`pwd`"
      else
        LD_LIBRARY_PATH="`pwd`:$LD_LIBRARY_PATH"
      fi
      export LD_LIBRARY_PATH
      if test -z "`./conftest1 2>&1`" ; then
        echo "checking for ${CXX} -shared ... yes"
        LD_SHARED="${CXX} -shared $IRIX_ALL"
      else
        echo "checking for ${CXX} -shared ... no(3)"
      fi
      LD_LIBRARY_PATH="$LD_LIBRARY_PATH_OLD"
    else
      echo "checking for ${CXX} -shared ... no(2)"
    fi
  else
    if test "$LD_SHARED" = "/bin/true" ; then
      echo "checking for ${CXX} -shared ... no(1)"
    fi
  fi

  if test "$LD_SHARED" = "/bin/true" \
          -a -z "`ld -shared conftest2.o -o libconftest.so 2>&1`" ; then
    if test -z "`${CC} conftest1.c libconftest.so -o conftest1 2>&1`"; then
      LD_LIBRARY_PATH_OLD="$LD_LIBRARY_PATH"
      if test -z "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="`pwd`"
      else
        LD_LIBRARY_PATH="`pwd`:$LD_LIBRARY_PATH"
      fi
      export LD_LIBRARY_PATH
      if test -z "`./conftest1 2>&1`" ; then
        echo "checking for ld -shared ... yes"
        LD_SHARED="ld -shared"
      fi
      LD_LIBRARY_PATH="$LD_LIBRARY_PATH_OLD"
    fi
  fi

  if test "$LD_SHARED" = "/bin/true" ; then
    echo "checking for ld -shared ... no"
    if test ! -x /bin/true ; then
      LD_SHARED=/usr/bin/true
    fi
  fi
  if test "$LD_SHARED" = "no" ; then
    if test -x /bin/true ; then
      LD_SHARED=/bin/true
    else
      LD_SHARED=/usr/bin/true
    fi
  fi

  rm -rf conftest* libconftest*

  AC_SUBST(LD_SHARED,$LD_SHARED)
  AC_SUBST(SO_EXT,$SO_EXT)
])

# --------------------------------------------------------
dnl AC_CHECK_FW_FUNC(FRAMEWORK-BASENAME, FUNCTION,
dnl              [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
dnl              [OTHER-LIBRARIES])
dnl ------------------------------------------------------
dnl
dnl Duplicate of AC_CHECK_LIB, with small edit to handle -framework $1, i.e.:
dnl   "-framework JavaVM" instead of "-ljvm"
dnl See autoconf-src/lib/autoconf/libs.m4 for more information

AC_DEFUN([AC_CHECK_FW_FUNC],
[m4_ifval([$3], , [AH_CHECK_LIB([$1])])dnl
AS_LITERAL_WORD_IF([$1],
	      [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1_$2])],
	      [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1''_$2])])dnl
AC_CACHE_CHECK([for $2 in -framework $1], [ac_Lib],
[ac_check_fw_func_save_LIBS=$LIBS
LIBS="-framework $1 $5 $LIBS"
AC_LINK_IFELSE([AC_LANG_CALL([], [$2])],
	       [AS_VAR_SET([ac_Lib], [yes])],
	       [AS_VAR_SET([ac_Lib], [no])])
LIBS=$ac_check_fw_func_save_LIBS])
AS_VAR_IF([ac_Lib], [yes],
      [m4_default([$3], [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1))
  LIBS="-framework $1 $LIBS"
])],
      [$4])
AS_VAR_POPDEF([ac_Lib])dnl
])# AC_CHECK_FW_FUNC

dnl ---------------------------------------------------------------------------
dnl Message output
dnl ---------------------------------------------------------------------------
AC_DEFUN([LOC_MSG],[
echo "$1"
])

AC_DEFUN([LOC_YES_NO],[if test -n "${$1}" ; then echo yes ; else echo no ; fi])

AC_DEFUN([LOC_MSG_USE],[
[echo "  $1: ]`LOC_YES_NO($2)`"])

