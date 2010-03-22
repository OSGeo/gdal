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

  echo 'int main() { long long off=0; }' >> conftest.c
  if test -z "`${CC} -o conftest conftest.c 2>&1`" ; then
    AC_DEFINE(HAVE_LONG_LONG, 1, [Define to 1, if your compiler supports long long data type])
    AC_MSG_RESULT([long long])
  else
    AC_MSG_RESULT([no])
  fi
  rm -f conftest*
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
# This function is primariliy added to facilite testing that 
# function prototypes are properly found such that functions can
# be compiled properly in C++.  In particular, we want to include
# the real include file, not internal define prototypes. 
#
# eg.
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

  if test x"$with_unix_stdio_64" = x"yes" ; then
    VSI_FTELL64=ftell64
    VSI_FSEEK64=fseek64
  fi

  if test x"$with_unix_stdio_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.cpp
    echo 'int main() { long long off=0; fseek64(NULL, off, SEEK_SET); off = ftell64(NULL); return 0; }' >> conftest.c
    if test -z "`${CC} -o conftest conftest.c 2>&1`" ; then
      with_unix_stdio_64=yes
      VSI_FTELL64=ftell64
      VSI_FSEEK64=fseek64
    fi
    rm -f conftest*
  fi

  dnl I use CXX in this one, to ensure that the prototypes are available. 
  dnl these functions seem to exist on Linux, but aren't normally defined
  dnl by stdio.h.  With CXX (C++) this becomes a fatal error.

  if test x"$with_unix_stdio_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { long long off=0; fseeko64(NULL, off, SEEK_SET); off = ftello64(NULL); return 0; }' >> conftest.c
    if test -z "`${CXX} -o conftest conftest.c 2>&1`" ; then
      with_unix_stdio_64=yes
      VSI_FTELL64=ftello64
      VSI_FSEEK64=fseeko64
    fi
    rm -f conftest*
  fi

  dnl This is much like the first test, but we predefine _LARGEFILE64_SOURCE 
  dnl before including stdio.h.  This should work on Linux 2.4 series systems.

  if test x"$with_unix_stdio_64" = x"" ; then
    echo '#define _LARGEFILE64_SOURCE' > conftest.c
    echo '#include <stdio.h>' >> conftest.c
    echo 'int main() { long long off=0; fseeko64(NULL, off, SEEK_SET); off = ftello64(NULL); return 0; }' >> conftest.c
    if test -z "`${CXX} -o conftest conftest.c 2>&1`" ; then
      with_unix_stdio_64=yes
      VSI_FTELL64=ftello64
      VSI_FSEEK64=fseeko64
      AC_DEFINE(VSI_NEED_LARGEFILE64_SOURCE, 1, [Define to 1, if you have LARGEFILE64_SOURCE])
    fi
    rm -f conftest*
  fi

  dnl Test special MacOS (Darwin) case. 

  if test x"$with_unix_stdio_64" = x"" ; then
    case "${host_os}" in
      darwin*)
        with_unix_stdio_64=yes
        VSI_FTELL64=ftello
        VSI_FSEEK64=fseeko
        ;;
    esac
  fi

  dnl Test for BSD systems that support ftello/fseeko.
  dnl OpenBSD throws warnings about using strcpy/strcat, so we use CC instead of CXX

  if test x"$with_unix_stdio_64" = x"" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { fpos_t off=0; fseeko(NULL, off, SEEK_SET); off = ftello(NULL); return 0; }' >> conftest.c
    if test -z "`${CC} -o conftest conftest.c 2>&1`" ; then
      with_unix_stdio_64=yes
      VSI_FTELL64=ftello
      VSI_FSEEK64=fseeko
    fi
    rm -f conftest*
  fi

  if test x"$with_unix_stdio_64" = x"yes" ; then
    AC_MSG_RESULT([yes])

    AC_CHECK_FUNC(stat64, VSI_STAT64=stat64 VSI_STAT64_T=stat64, VSI_STAT64=stat VSI_STAT64_T=stat)
    AC_CHECK_FUNC(fopen64, VSI_FOPEN64=fopen64, VSI_FOPEN64=fopen)

    AC_DEFINE(UNIX_STDIO_64, 1, [Define to 1 if you have fseek64, ftell64])
    AC_DEFINE(VSI_LARGE_API_SUPPORTED, 1, [Define to 1, if you have 64 bit STDIO API])

    export VSI_FTELL64 VSI_FSEEK64 VSI_STAT64 VSI_STAT64_T VSI_OPEN64
    AC_DEFINE_UNQUOTED(VSI_FTELL64,$VSI_FTELL64, [Define to name of 64bit ftell func])
    AC_DEFINE_UNQUOTED(VSI_FSEEK64,$VSI_FSEEK64, [Define to name of 64bit fseek func])
    AC_DEFINE_UNQUOTED(VSI_STAT64,$VSI_STAT64, [Define to name of 64bit stat function])
    AC_DEFINE_UNQUOTED(VSI_STAT64_T,$VSI_STAT64_T, [Define to name of 64bit stat structure])
    AC_DEFINE_UNQUOTED(VSI_FOPEN64,$VSI_FOPEN64, [Define to name of 64bit fopen function])
  else
    AC_MSG_RESULT([no])
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
	rm -f conftest*
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

  rm -f conftest* libconftest* 

  AC_SUBST(LD_SHARED,$LD_SHARED)
  AC_SUBST(SO_EXT,$SO_EXT)
])

dnl
dnl Find Python.
dnl

AC_DEFUN([AM_PATH_PYTHON],
[
    dnl
    dnl Check for Python executable in PATH
    dnl
    AC_CHECK_PROGS([PYTHON], [python python1.5 python1.4 python1.3], [no])

dnl    if test "$with_ogpython" = no ; then
dnl        echo "Old-gen Python support disabled"
dnl        PYTHON=no
dnl    fi

dnl    if test "x$with_python" != xno -a "x$with_python" != "x" ; then
dnl        echo "Old-gen Python support disabled since python enabled."
dnl        PYTHON=no
dnl    fi

    ARCH=`uname -i 2>/dev/null`
    PYLIB=lib
    if test "$ARCH" = "x86_64" ; then
        PYLIB=lib64
    fi

    if test "$PYTHON" != no ; then
        AC_MSG_CHECKING([for location of Python Makefiles])

        changequote(,)dnl
        python_prefix="`$PYTHON -c '
import sys
print sys.prefix'`"
        changequote([, ])dnl
        
        changequote(,)dnl
        python_execprefix="`$PYTHON -c '
import sys
print sys.prefix'`"
        changequote([, ])dnl

        changequote(,)dnl
        python_version="`$PYTHON -c '
import sys
print sys.version[:3]'`"
        changequote([, ])dnl

        pythondir=$python_prefix/$PYLIB/python${python_version}/site-packages
        pyexecdir=$python_execprefix/$PYLIB/python${python_version}/site-packages

        AC_MSG_RESULT(found)

        dnl Verify that we have the makefile needed later.
    
        dnl AC_MSG_CHECKING([for top-level Makefile for Python])
        dnl py_mf=$python_execprefix/$PYLIB/python${python_version}/config/Makefile
        dnl if test -f $py_mf ; then
        dnl     AC_MSG_RESULT(found)
        dnl else
        dnl     AC_MSG_RESULT([missing, Old-gen Python disabled.])
        dnl     PYTHON=no
        dnl fi
    else
        dnl These defaults are version independent
        pythondir='$(prefix)/lib/site-python'
        pyexecdir='$(exec_prefix)/lib/site-python'
    fi

    dnl TODO: Add HELP_STRING
    AC_ARG_WITH([pymoddir],[  --with-pymoddir=ARG   Override Old-gen Python package install dir],,)

    if test "$PYTHON" != "no" ; then
        AC_MSG_CHECKING([where to install Python modules])

        if test "$with_pymoddir" = "" ; then
            pymoddir=$pyexecdir
        else
            pymoddir=$with_pymoddir
        fi

        export pymoddir
        AC_MSG_RESULT([$pymoddir])
    fi

    AC_SUBST([pythondir])
    AC_SUBST([pyexecdir])
    AC_SUBST([pymoddir])

]) dnl AM_PATH_PYTHON


dnl finds information needed for compilation of shared library style python
dnl extensions.  AM_PATH_PYTHON should be called before hand.
dnl NFW: Modified from original to avoid overridding CC, SO and OPT

AC_DEFUN([AM_INIT_PYEXEC_MOD],
[
    AC_REQUIRE([AM_PATH_PYTHON])
    PYTHON_LIBS=""
    PYTHON_DEV="no"

    if test "$PYTHON" != no ; then
        AC_MSG_CHECKING([for location of Python headers])

        AC_CACHE_VAL(am_cv_python_includes,
        [
        changequote(,)dnl
        am_cv_python_includes="`$PYTHON -c '
import sys
includepy = \"%s/include/python%s\" % (sys.prefix, sys.version[:3])
if sys.version[0] > \"1\" or sys.version[2] > \"4\":
    libpl = \"%s/include/python%s\" % (sys.exec_prefix, sys.version[:3])
else:
    libpl = \"%s/'$PYLIB'/python%s/config\" % (sys.exec_prefix, sys.version[:3])
print \"-I%s -I%s\" % (includepy, libpl)'`"
        changequote([, ])
        ])

        PYTHON_INCLUDES="$am_cv_python_includes"
        AC_MSG_RESULT(found)
        AC_SUBST(PYTHON_INCLUDES)

        AC_MSG_CHECKING([definitions from top-level Python Makefile])
        AC_CACHE_VAL(am_cv_python_makefile,
        [
        changequote(,)dnl
        if test ! -z "`uname -a | grep CYGWIN`" ; then 
            PYTHON_LIBS="`$PYTHON -c '
import sys
print \"-L%s/'$PYLIB'/python%s/config -lpython%s.dll\" % (sys.prefix, sys.version[:3], sys.version[:3])'`"
        fi 
        
        py_makefile="`$PYTHON -c '
import sys
print \"%s/'$PYLIB'/python%s/config/Makefile\"%(sys.exec_prefix, sys.version[:3])'`"

        if test ! -f "$py_makefile"; then
            echo Could not find the python config makefile.  Maybe you are;
            echo missing the development portion of the python installation;
            exit;
        fi

        eval `sed -n \
            -e "s/^CC=[ 	]*\(.*\)/am_cv_python_CC='\1'/p" \
            -e "s/^OPT=[ 	]*\(.*\)/am_cv_python_OPT='\1'/p" \
            -e "s/^CCSHARED=[ 	]*\(.*\)/am_cv_python_CCSHARED='\1'/p" \
            -e "s/^LDSHARED=[ 	]*\(.*\)/am_cv_python_LDSHARED='\1'/p" \
            -e "s/^SO=[ 	]*\(.*\)/am_cv_python_SO='\1'/p" \
            $py_makefile`
        
        am_cv_python_makefile=found
        changequote([, ])

        AC_MSG_RESULT([$am_cv_python_makefile])
        ]) dnl AC_CACHE_VAL

        dnl Check if everything compiles well
        AC_MSG_CHECKING([for Python.h header])
        AC_LANG_PUSH([C])

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$PYTHON_INCLUDES"
        
        AC_COMPILE_IFELSE(
            [#include <Python.h>],
            [ac_cv_python_dev_exists=yes],
            [ac_cv_python_dev_exists=no]
        )

        AC_LANG_POP()
        CPPFLAGS="$saved_CPPFLAGS"

        if test ! "$ac_cv_python_dev_exists" = "yes"; then
            AC_MSG_RESULT([not found])
            PYTHON_DEV="no"

            AC_MSG_WARN([
*** Could not compile test program with Python.h included, so Python bindings for GDAL will not be built.
*** Check if you have installed development version of the Python package for your distribution.])

        else
            AC_MSG_RESULT([found])
            PYTHON_DEV="yes"

            PYTHON_CC="$am_cv_python_CC"
            PYTHON_OPT="$am_cv_python_OPT"
            PYTHON_SO="$am_cv_python_SO"
            PYTHON_CFLAGS="$am_cv_python_CCSHARED \$(OPT)"
            PYTHON_LINK="$am_cv_python_LDSHARED -o \[$]@"
        fi
    fi

    if test "$PYTHON_DEV" = "yes"; then
        AC_MSG_CHECKING([for special pymod link hacks])
        if test ! -z "`uname | grep Darwin`" -a ${with_libtool} == no ; then
            AC_MSG_RESULT([darwin-nonlibtool])
            PY_LD_SHARED='g++ -bundle -framework Python'
            PY_SO_EXT='so'
        elif test ! -z "`uname | grep Darwin`" -a ${with_libtool} == yes ; then
            AC_MSG_RESULT([darwin-libtool])
            PYTHON_LIBS='-XCClinker -framework -XCClinker Python $(LIBS)'
            PY_LD_SHARED='$(LD_SHARED)'
            PY_SO_EXT='$(SO_EXT)'
        else
            AC_MSG_RESULT([default])
            PY_LD_SHARED='$(LD_SHARED)'
            PY_SO_EXT='$(SO_EXT)'
        fi
    else
        PYTHON_CC=""
        PYTHON_OPT=""
        PYTHON_SO=""
        PYTHON_CFLAGS=""
        PYTHON_LINK=""
        PYTHON_LIBS=""
        PY_LD_SHARED=""
        PY_SO_EXT=""
    fi


    export PY_LD_SHARED PY_SO_EXT

    AC_SUBST([PYTHON_CC])
    AC_SUBST([PYTHON_OPT])
    AC_SUBST([PYTHON_SO])
    AC_SUBST([PYTHON_CFLAGS])
    AC_SUBST([PYTHON_LIBS])
    AC_SUBST([PYTHON_LINK])
    AC_SUBST([PY_LD_SHARED])
    AC_SUBST([PY_SO_EXT])

    dnl Main flag indicating if Python development package has been found.
    dnl PYTHON and PYTHON_DEV are used together to decide about building Python bindings for GDAL.
    AC_SUBST([PYTHON_DEV])

]) dnl AM_INIT_PYEXEC_MOD


dnl
dnl Check if we have NUMPY include file(s).
dnl

AC_DEFUN([AM_CHECK_NUMPY],
[
  AC_MSG_CHECKING([for Python NumPy headers])

  echo '#include "Python.h"' > conftest.c
  echo '#include "Numeric/arrayobject.h"' >> conftest.c
  if test -z "`${CC-cc} $PYTHON_INCLUDES -c conftest.c 2>&1`"; then
    HAVE_NUMPY=yes
    AC_MSG_RESULT([found])
  else
    HAVE_NUMPY=no
    AC_MSG_RESULT([not found])
  fi
  export HAVE_NUMPY
  rm -f conftest.c

  AC_SUBST([HAVE_NUMPY])
  if test "$HAVE_NUMPY" = "yes" ; then
    NUMPY_FLAG=-DHAVE_NUMPY
  else
    NUMPY_FLAG=-UHAVE_NUMPY
  fi
  export NUMPY_FLAG
  AC_SUBST([NUMPY_FLAG])
])


dnl ---------------------------------------------------------------------------
dnl Message output
dnl ---------------------------------------------------------------------------
AC_DEFUN([LOC_MSG],[
echo "$1"
])

AC_DEFUN([LOC_YES_NO],[if test -n "${$1}" ; then echo yes ; else echo no ; fi])

AC_DEFUN([LOC_MSG_USE],[
[echo "  $1: ]`LOC_YES_NO($2)`"])

