dnl ---------------------------------------------------------------------------
dnl Try to establish a 64 bit integer type.
dnl ---------------------------------------------------------------------------

AC_DEFUN(AC_HAVE_LONG_LONG,
[
  AC_MSG_CHECKING([for 64bit integer type])

  echo 'int main() { long long off=0; }' >> conftest.c
  if test -z "`${CC} -o conftest conftest.c 2>&1`" ; then
    AC_DEFINE(HAVE_LONG_LONG)
    AC_MSG_RESULT([long long])
  else
    AC_MSG_RESULT([no])
  fi
  rm -f conftest*
])

dnl ---------------------------------------------------------------------------
dnl Check for Unix 64 bit STDIO API (fseek64, ftell64 like on IRIX).
dnl ---------------------------------------------------------------------------

AC_DEFUN(AC_UNIX_STDIO_64,
[
  AC_ARG_WITH(unix_stdio_64,[  --with-unix-stdio-64[=ARG] Utilize 64 stdio api - yes/no)],,)

  AC_MSG_CHECKING([for 64bit file io])

  if test "$with_unix_stdio_64" = "" ; then
    echo '#include <stdio.h>' > conftest.c
    echo 'int main() { long long off=0; fseek64(NULL, SEEK_SET, off); off = ftell64(NULL); return 0; }' >> conftest.c
    if test -z "`${CC} -o conftest conftest.c 2>&1`" ; then
      with_unix_stdio_64=yes
    fi
    rm -f conftest*
  fi

  if test "$with_unix_stdio_64" = "yes" ; then
    AC_MSG_RESULT([yes])

    AC_DEFINE(UNIX_STDIO_64)
    AC_DEFINE(VSI_LARGE_API_SUPPORTED)
  else
    AC_MSG_RESULT([no])
  fi

])

AC_DEFUN(AC_COMPILER_WFLAGS,
[
	# Remove -g from compile flags, we will add via CFG variable if
	# we need it.
	CXXFLAGS=`echo "$CXXFLAGS " | sed "s/-g //"`
	CFLAGS=`echo "$CFLAGS " | sed "s/-g //"`

	# check for GNU compiler, and use -Wall
	if test "$GCC" = "yes"; then
		C_WFLAGS="-Wall"
		AC_DEFINE(USE_GNUCC)
	fi
	if test "$GXX" = "yes"; then
		CXX_WFLAGS="-Wall"
		AC_DEFINE(USE_GNUCC)
	fi
	AC_SUBST(CXX_WFLAGS,$CXX_WFLAGS)
	AC_SUBST(C_WFLAGS,$C_WFLAGS)
])

AC_DEFUN(AC_COMPILER_PIC,
[
	echo 'void f(){}' > conftest.c
	if test -z "`${CC-cc} -fPIC -c conftest.c 2>&1`"; then
	  C_PIC=-fPIC
	else
	  C_PIC=
	fi
	if test -z "`${CXX-g++} -fPIC -c conftest.c 2>&1`"; then
	  CXX_PIC=-fPIC
	else
	  CXX_PIC=
	fi
	rm -f conftest*

	AC_SUBST(CXX_PIC,$CXX_PIC)
	AC_SUBST(C_PIC,$C_PIC)
])

dnl
dnl Look for OGDI, and verify that we can link and run.
dnl
AC_DEFUN(AC_TRY_OGDI,
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
AC_DEFUN(AC_LD_SHARED,
[
  echo 'void g(); int main(){ g(); return 0; }' > conftest1.c

  echo 'void g(); void g(){}' > conftest2.c
  ${CC} ${C_PIC} -c conftest2.c

  SO_EXT="so"
  export SO_EXT
  LD_SHARED="/bin/true"
  if test -z "`${CXX} -shared conftest2.o -o libconftest.so 2>&1`" ; then
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
        LD_SHARED="${CXX} -shared"
      else
        echo "checking for ${CXX} -shared ... no(3)"
      fi
      LD_LIBRARY_PATH="$LD_LIBRARY_PATH_OLD"
    else
      echo "checking for ${CXX} -shared ... no(2)"
    fi
  else
    echo "checking for ${CXX} -shared ... no(1)"
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

  if test "$LD_SHARED" = "/bin/true" \
          -a -z "`${CC} -dynamiclib conftest2.o -o libconftest.so 2>&1`" ; then
    if test -z "`${CC} conftest1.c libconftest.so -o conftest1 2>&1`"; then
      DYLD_LIBRARY_PATH_OLD="$DYLD_LIBRARY_PATH"
      if test -z "$DYLD_LIBRARY_PATH" ; then
        DYLD_LIBRARY_PATH="`pwd`"
      else
        DYLD_LIBRARY_PATH="`pwd`:$DYLD_LIBRARY_PATH"
      fi
      export DYLD_LIBRARY_PATH
      if test -z "`./conftest1 2>&1`" ; then
        echo "checking for ${CC} -dynamiclib ... yes"
        LD_SHARED="${CC} -dynamiclib"
	SO_EXT=dylib
      fi
      DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH_OLD"
    fi
  fi

  if test "$LD_SHARED" = "/bin/true" ; then
    echo "checking for ld -shared ... no"
    if test ! -x /bin/true ; then
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

AC_DEFUN(AM_PATH_PYTHON,
  [AC_CHECK_PROGS(PYTHON, python python1.5 python1.4 python1.3,no)
  if test "$with_python" = no ; then
     echo "python support disabled"
     PYTHON=no
  fi
  if test "$PYTHON" != no ; then
    AC_MSG_CHECKING([where .py files should go])
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

    pythondir=$python_prefix/lib/python${python_version}/site-packages
    pyexecdir=$python_execprefix/lib/python${python_version}/site-packages

    AC_MSG_RESULT(found)

    dnl Verify that we have the makefile needed later.
    
    AC_MSG_CHECKING([Python makefile])
    py_mf=$python_execprefix/lib/python${python_version}/config/Makefile
    if test -f $py_mf ; then
      AC_MSG_RESULT(found)
    else
      AC_MSG_RESULT([missing, python disabled.])
      PYTHON=no
    fi
  else
    # these defaults are version independent ...
    pythondir='$(prefix)/lib/site-python'
    pyexecdir='$(exec_prefix)/lib/site-python'
  fi
  AC_SUBST(pythondir)
  AC_SUBST(pyexecdir)])


dnl finds information needed for compilation of shared library style python
dnl extensions.  AM_PATH_PYTHON should be called before hand.
dnl NFW: Modified from original to avoid overridding CC, SO and OPT

AC_DEFUN(AM_INIT_PYEXEC_MOD,
  [AC_REQUIRE([AM_PATH_PYTHON])
if test "$PYTHON" != no ; then
  AC_MSG_CHECKING([for python headers])
  AC_CACHE_VAL(am_cv_python_includes,
    [changequote(,)dnl
    am_cv_python_includes="`$PYTHON -c '
import sys
includepy = \"%s/include/python%s\" % (sys.prefix, sys.version[:3])
if sys.version[0] > \"1\" or sys.version[2] > \"4\":
  libpl = \"%s/include/python%s\" % (sys.exec_prefix, sys.version[:3])
else:
  libpl = \"%s/lib/python%s/config\" % (sys.exec_prefix, sys.version[:3])
print \"-I%s -I%s\" % (includepy, libpl)'`"
    changequote([, ])])
  PYTHON_INCLUDES="$am_cv_python_includes"
  AC_MSG_RESULT(found)
  AC_SUBST(PYTHON_INCLUDES)

  AC_MSG_CHECKING([definitions from Python makefile])
  AC_CACHE_VAL(am_cv_python_makefile,
    [changequote(,)dnl
    py_makefile="`$PYTHON -c '
import sys
print \"%s/lib/python%s/config/Makefile\"%(sys.exec_prefix, sys.version[:3])'`"
    if test ! -f "$py_makefile"; then
      AC_MSG_ERROR([*** Couldn't find the python config makefile.  Maybe you are
*** missing the development portion of the python installation])
    fi
    eval `sed -n \
-e "s/^CC=[ 	]*\(.*\)/am_cv_python_CC='\1'/p" \
-e "s/^OPT=[ 	]*\(.*\)/am_cv_python_OPT='\1'/p" \
-e "s/^CCSHARED=[ 	]*\(.*\)/am_cv_python_CCSHARED='\1'/p" \
-e "s/^LDSHARED=[ 	]*\(.*\)/am_cv_python_LDSHARED='\1'/p" \
-e "s/^SO=[ 	]*\(.*\)/am_cv_python_SO='\1'/p" \
    $py_makefile`
    am_cv_python_makefile=found
    changequote([, ])])
  AC_MSG_RESULT(done)
  PYTHON_CC="$am_cv_python_CC"
  PYTHON_OPT="$am_cv_python_OPT"
  PYTHON_SO="$am_cv_python_SO"
  PYTHON_CFLAGS="$am_cv_python_CCSHARED \$(OPT)"
  PYTHON_LINK="$am_cv_python_LDSHARED -o \[$]@"
else
  PYTHON_CC=""
  PYTHON_OPT=""
  PYTHON_SO=""
  PYTHON_CFLAGS=""
  PYTHON_LINK=""
fi
AC_SUBST(PYTHON_CC)dnl
AC_SUBST(PYTHON_OPT)dnl
AC_SUBST(PYTHON_SO)dnl
AC_SUBST(PYTHON_CFLAGS)dnl
AC_SUBST(PYTHON_LINK)])

dnl
dnl Check if we have NUMPY include file(s).
dnl

AC_DEFUN(AM_CHECK_NUMPY,
  [
  AC_MSG_CHECKING([for NumPy include files])
  echo '#include "Python.h"' > conftest.c
  echo '#include "Numeric/arrayobject.h"' >> conftest.c
  if test -z "`${CC-cc} $PYTHON_INCLUDES -c conftest.c 2>&1`"; then
    HAVE_NUMPY=yes
    AC_MSG_RESULT(found)
  else
    HAVE_NUMPY=no
    AC_MSG_RESULT(missing)
  fi
  export HAVE_NUMPY
  rm -f conftest.c

  AC_SUBST(HAVE_NUMPY)
  if test "$HAVE_NUMPY" = "yes" ; then
    NUMPY_FLAG=-DHAVE_NUMPY
  else
    NUMPY_FLAG=
  fi
  export NUMPY_FLAG
  AC_SUBST(NUMPY_FLAG)])



