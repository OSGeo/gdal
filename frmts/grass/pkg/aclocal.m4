AC_DEFUN(AC_COMPILER_LOCALHACK,
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

AC_DEFUN(AC_COMPILER_WFLAGS,
[
	# Remove -g from compile flags, we will add via CFG variable if
	# we need it.
	CXXFLAGS=`echo "$CXXFLAGS " | sed "s/-g //"`
	CFLAGS=`echo "$CFLAGS " | sed "s/-g //"`

	# check for GNU compiler, and use -Wall
	if test "$GCC" = "yes"; then
		C_WFLAGS="-Wall"
		AC_DEFINE(USE_GNUCC, 1, [Define to 1, if you have GNU C
		compiler])
	fi
	if test "$GXX" = "yes"; then
		CXX_WFLAGS="-Wall"
		AC_DEFINE(USE_GNUCC, 1, [Define to 1, if you have GNU C
		compiler])
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
dnl Try to find something to link shared libraries with.  Use "c++ -shared"
dnl in preference to "ld -shared" because it will link in required c++
dnl run time support for us. 
dnl
AC_DEFUN(AC_LD_SHARED,
[
  echo 'void g(); int main(){ g(); return 0; }' > conftest1.c

  echo '#include <stdio.h>' > conftest2.c
  echo 'void g(); void g(){printf("");}' >> conftest2.c
  ${CC} ${C_PIC} -c conftest2.c

  SO_EXT="so"
  export SO_EXT
  LD_SHARED="/bin/true"
  if test ! -z "`uname -a | grep IRIX`" ; then
    IRIX_ALL=-all
  else
    IRIX_ALL=
  fi

  AC_ARG_WITH(ld-shared,[  --with-ld-shared=cmd    provide shared library link],,)

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
