dnl $Id$
dnl
dnl @synopsis AX_LIB_LIBKML([MINIMUM-VERSION])
dnl
dnl This macro provides tests of availability of Google libkml library
dnl of particular version or newer.
dnl
dnl libkml - http://code.google.com/p/libkml/
dnl
dnl This macros checks for Google libkml headers and libraries 
dnl and defines compilation flags.
dnl 
dnl Macro supports following options and their values:
dnl 1) Single-option usage:
dnl --with-libkml - yes, no or path to Google libkml installation prefix
dnl 2) Three-options usage (all options are required):
dnl --with-libkml=yes
dnl --with-libkml-inc - path to base directory with  headers
dnl --with-libkml-lib - linker flags for 
dnl
dnl This macro calls:
dnl
dnl   AC_SUBST(LIBKML_CFLAGS)
dnl   AC_SUBST(LIBKML_LDFLAGS)
dnl   AC_SUBST(LIBKML_VERSION) - only if version requirement is used
dnl
dnl And sets:
dnl
dnl   HAVE_LIBKML
dnl
dnl @category InstalledPackages
dnl @category Cxx
dnl @author Mateusz Loskot <mateusz@loskot.net>
dnl @version $Date$
dnl @license AllPermissive
dnl          Copying and distribution of this file, with or without modification,
dnl          are permitted in any medium without royalty provided the copyright notice and
dnl          this notice are preserved.
dnl
AC_DEFUN([AX_LIB_LIBKML],
[
    AC_ARG_WITH([libkml],
        AC_HELP_STRING([--with-libkml=@<:@ARG@:>@],
            [use Google libkml from given prefix (ARG=path); check standard prefixes (ARG=yes); disable (ARG=no)]
        ),
        [
        if test "$withval" = "yes"; then
            if test -d /usr/local/include/kml ; then 
                libkml_prefix=/usr/local
            elif test -d /usr/include/kml ; then
                libkml_prefix=/usr
            else
                libkml_prefix=""
            fi
            libkml_requested="yes"
        elif test -d "$withval"; then
            libkml_prefix="$withval"
            libkml_requested="yes"
        else
            libkml_prefix=""
            libkml_requested="no"
        fi
        ],
        [
        dnl Default behavior is implicit yes
        if test -d /usr/local/include/kml ; then 
            libkml_prefix=/usr/local
        elif test -d /usr/include/kml ; then
            libkml_prefix=/usr
        else
            libkml_prefix="" 
        fi
        ]
    )

    AC_ARG_WITH([libkml-inc],
        AC_HELP_STRING([--with-libkml-inc=@<:@DIR@:>@],
            [path to Google libkml headers]
        ),
        [libkml_include_dir="$withval"],
        [libkml_include_dir=""]
    )
    AC_ARG_WITH([libkml-lib],
        AC_HELP_STRING([--with-libkml-lib=@<:@ARG@:>@],
            [link options for Google libkml libraries]
        ),
        [libkml_lib_flags="$withval"],
        [libkml_lib_flags=""]
    )

    LIBKML_CFLAGS=""
    LIBKML_LDFLAGS=""
    LIBKML_VERSION=""

    dnl
    dnl Collect include/lib paths and flags
    dnl 
    run_libkml_test="no"

    if test -n "$libkml_prefix"; then
        libkml_include_dir="$libkml_prefix/include"
        libkml_include_dir2="$libkml_prefix/include/kml"
        libkml_include_dir3="$libkml_prefix/include/kml/third_party/boost_1_34_1"
        if test "$libkml_prefix" = "/usr"; then
            libkml_lib_flags="-lkmlengine -lkmldom -lkmlbase -lkmlconvenience"
        else
            libkml_lib_flags="-L$libkml_prefix/lib -lkmlengine -lkmldom -lkmlbase -lkmlconvenience"
        fi
        run_libkml_test="yes"
    elif test "$libkml_requested" = "yes"; then
        if test -n "$libkml_include_dir" -a -n "$libkml_lib_flags"; then
            libkml_include_dir2="$libkml_include_dir/kml"
            libkml_include_dir3="$libkml_include_dir/kml/third_party/boost_1_34_1"
            run_libkml_test="yes"
        fi
    else
        run_libkml_test="no"
    fi

    dnl
    dnl Check libkml headers/libraries
    dnl
    if test "$run_libkml_test" = "yes"; then

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS -I$libkml_include_dir -I$libkml_include_dir2 -I$libkml_include_dir3"

        saved_LDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS $libkml_lib_flags"

        dnl
        dnl Check headers
        dnl
        AC_MSG_CHECKING([for Google libkml headers in $libkml_include_dir, $libkml_include_dir2, and $libkml_include_dir3])

        AC_LANG_PUSH([C++])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM(
                [[
@%:@include <kml/dom.h>
                ]],
                [[]]
            )],
            [
            LIBKML_CFLAGS="-I$libkml_include_dir -I$libkml_include_dir2 -I$libkml_include_dir3"
            libkml_header_found="yes"
            AC_MSG_RESULT([found])
            ],
            [
            libkml_header_found="no"
            AC_MSG_RESULT([not found])
            ]
        )
        AC_LANG_POP([C++])
        
        dnl
        dnl Check libraries
        dnl
        if test "$libkml_header_found" = "yes"; then

            AC_MSG_CHECKING([for Google libkml libraries])

            AC_LANG_PUSH([C++])
            AC_LINK_IFELSE([
                AC_LANG_PROGRAM(
                    [[
@%:@include <kml/dom.h>
                    ]],
                    [[
kmldom::KmlFactory* factory = kmldom::KmlFactory::GetFactory();
                    ]]
                )],
                [
                LIBKML_LDFLAGS="$libkml_lib_flags"
                libkml_lib_found="yes"
                AC_MSG_RESULT([found])
                ],
                [
                libkml_lib_found="no"
                AC_MSG_RESULT([not found])
                ]
            )
            AC_LANG_POP([C++])
        fi

        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDFLAGS"
    fi

    AC_MSG_CHECKING([for Google libkml])

    if test "$run_libkml_test" = "yes"; then
        if test "$libkml_header_found" = "yes" -a "$libkml_lib_found" = "yes"; then

            AC_SUBST([LIBKML_CFLAGS])
            AC_SUBST([LIBKML_LDFLAGS])

            HAVE_LIBKML="yes"
        else 
            HAVE_LIBKML="no"
        fi

        AC_MSG_RESULT([$HAVE_LIBKML])

        dnl
        dnl Check  version
        dnl
        if test "$HAVE_LIBKML" = "yes"; then

            libkml_version_req=ifelse([$1], [], [], [$1])
            
            if test  -n "$libkml_version_req"; then

                AC_MSG_CHECKING([if Google libkml version is >= $libkml_version_req])

                if test -f "$libkml_include_dir2/base/version.h"; then

                    libkml_major=$(sed -n '/^#define.\+LIBKML_MAJOR_VERSION.\+[[:digit:]]\+$/ {s/^[^[:digit:]]\+\([[:digit:]]\+\)$/\1/; P}' \
		    	$libkml_include_dir2/base/version.h)
                    libkml_minor=$(sed -n '/^#define.\+LIBKML_MINOR_VERSION.\+[[:digit:]]\+$/ {s/^[^[:digit:]]\+\([[:digit:]]\+\)$/\1/; P}' \
		    	$libkml_include_dir2/base/version.h)
                    libkml_revision=$(sed -n '/^#define.\+LIBKML_MICRO_VERSION.\+[[:digit:]]\+$/ {s/^[^[:digit:]]\+\([[:digit:]]\+\)$/\1/; P}' \
		    	$libkml_include_dir2/base/version.h)

                    LIBKML_VERSION="$libkml_major.$libkml_minor.$libkml_revision"
                    AC_SUBST([LIBKML_VERSION])

                    dnl Decompose required version string and calculate numerical representation
                    libkml_version_req_major=`expr $libkml_version_req : '\([[0-9]]*\)'`
                    libkml_version_req_minor=`expr $libkml_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
                    libkml_version_req_revision=`expr $libkml_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
                    if test "x$libkml_version_req_revision" = "x"; then
                        libkml_version_req_revision="0"
                    fi
                    
                    libkml_version_req_number=`expr $libkml_version_req_major \* 10000 \
                                               \+ $libkml_version_req_minor \* 100 \
                                               \+ $libkml_version_req_revision`

                    dnl Calculate numerical representation of detected version
                    libkml_version_number=`expr $libkml_major \* 10000 \
                                          \+ $libkml_minor \* 100 \
                                           \+ $libkml_revision`

                    libkml_version_check=`expr $libkml_version_number \>\= $libkml_version_req_number`
                    if test "$libkml_version_check" = "1"; then
                        AC_MSG_RESULT([yes])
                    else
                        AC_MSG_RESULT([no])
                        AC_MSG_WARN([Found Google libkml ${LIBKML_VERSION}, which is older than required (${libkml_version_req}). KML support disabled.])
			HAVE_LIBKML="no"
                    fi
                else
                    AC_MSG_RESULT([no])
                    AC_MSG_WARN([Missing header $libkml_include_dir2/base/bersion.hpp. Unable to determine Google libkml version.])
                fi
            fi
        fi

    else
        HAVE_LIBKML="no"
        AC_MSG_RESULT([$HAVE_LIBKML])

        if test "$libkml_requested" = "yes"; then
            AC_MSG_WARN([Google libkml support requested but headers or library not found. Specify valid prefix of libkml using --with-libkml=@<:@DIR@:>@ or provide include directory and linker flags using --with-libkml-inc and --with-libkml-lib])
        fi
    fi
])
