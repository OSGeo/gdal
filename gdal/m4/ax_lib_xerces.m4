dnl $Id$
dnl
dnl @synopsis AX_LIB_XERCES([MINIMUM-VERSION])
dnl
dnl This macro provides tests of availability of Apache Xerces C++ Parser
dnl of particular version or newer.
dnl This macros checks for Apache Xerces C++ Parser headers and libraries
dnl and defines compilation flags
dnl
dnl Macro supports following options and their values:
dnl 1) Single-option usage:
dnl --with-xerces - yes, no or path to Xerces C++ Parser installation prefix
dnl 2) Three-options usage (all options are required):
dnl --with-xerces=yes
dnl --with-xerces-inc - path to base directory with Xerces headers
dnl --with-xerces-lib - linker flags for Xerces
dnl
dnl This macro calls:
dnl
dnl   AC_SUBST(XERCES_CFLAGS)
dnl   AC_SUBST(XERCES_LDFLAGS)
dnl   AC_SUBST(XERCES_VERSION) - only if version requirement is used
dnl
dnl And sets:
dnl
dnl   HAVE_XERCES
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
AC_DEFUN([AX_LIB_XERCES],
[
    AC_ARG_WITH([xerces],
        AC_HELP_STRING([--with-xerces=@<:@ARG@:>@],
            [use Xerces C++ Parser from given prefix (ARG=path); check standard prefixes (ARG=yes); disable (ARG=no)]
        ),
        [
        if test "$withval" = "yes"; then
            if test -d /usr/local/include/xercesc ; then
                xerces_prefix=/usr/local
            elif test -d /usr/include/xercesc ; then
                xerces_prefix=/usr
            else
                xerces_prefix=""
            fi
            xerces_requested="yes"
        elif test -d "$withval"; then
            xerces_prefix="$withval"
            xerces_requested="yes"
        else
            xerces_prefix=""
            xerces_requested="no"
        fi
        ],
        [
        dnl Default behavior is implicit yes
        if test -d /usr/local/include/xercesc ; then
            xerces_prefix=/usr/local
        elif test -d /usr/include/xercesc ; then
            xerces_prefix=/usr
        else
            xerces_prefix=""
        fi
        ]
    )

    AC_ARG_WITH([xerces-inc],
        AC_HELP_STRING([--with-xerces-inc=@<:@DIR@:>@],
            [path to Xerces C++ Parser headers]
        ),
        [xerces_include_dir="$withval"],
        [xerces_include_dir=""]
    )
    AC_ARG_WITH([xerces-lib],
        AC_HELP_STRING([--with-xerces-lib=@<:@ARG@:>@],
            [link options for Xerces C++ Parser libraries]
        ),
        [xerces_lib_flags="$withval"],
        [xerces_lib_flags=""]
    )

    XERCES_CFLAGS=""
    XERCES_LDFLAGS=""
    XERCES_VERSION=""

    dnl
    dnl Collect include/lib paths and flags
    dnl
    run_xerces_test="no"

    if test -n "$xerces_prefix" -a -z "$xerces_include_dir" -a -z "$xerces_lib_flags"; then
        xerces_include_dir="$xerces_prefix/include"
        xerces_include_dir2="$xerces_prefix/include/xercesc"
        if test "$xerces_prefix" = "/usr"; then
            xerces_lib_flags="-lxerces-c -lpthread"
        else
            xerces_lib_flags="-L$xerces_prefix/lib -lxerces-c -lpthread"
        fi
        run_xerces_test="yes"
    elif test "$xerces_requested" = "yes"; then
        if test -n "$xerces_include_dir" -a -n "$xerces_lib_flags"; then
            xerces_include_dir2="$xerces_include_dir/xercesc"
            run_xerces_test="yes"
        fi
    else
        run_xerces_test="no"
    fi

    dnl
    dnl Check Xerces C++ Parser files
    dnl
    if test "$run_xerces_test" = "yes"; then

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS -I$xerces_include_dir -I$xerces_include_dir2"

        saved_LIBS="$LIBS"
        LIBS="$xerces_lib_flags $LIBS"

        dnl
        dnl Check Xerces headers
        dnl
        AC_MSG_CHECKING([for Xerces C++ Parser headers in $xerces_include_dir and $xerces_include_dir2])

        AC_LANG_PUSH([C++])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM(
                [[
@%:@include <xercesc/util/XercesDefs.hpp>
@%:@include <xercesc/util/PlatformUtils.hpp>
                ]],
                [[]]
            )],
            [
            XERCES_CFLAGS="-I$xerces_include_dir -I$xerces_include_dir2"
            xerces_header_found="yes"
            AC_MSG_RESULT([found])
            ],
            [
            xerces_header_found="no"
            AC_MSG_RESULT([not found])
            ]
        )
        AC_LANG_POP([C++])

        dnl
        dnl Check Xerces libraries
        dnl
        if test "$xerces_header_found" = "yes"; then

            AC_MSG_CHECKING([for Xerces C++ Parser libraries])

            AC_LANG_PUSH([C++])
            AC_LINK_IFELSE([
                AC_LANG_PROGRAM(
                    [[
@%:@include <xercesc/util/XercesDefs.hpp>
@%:@include <xercesc/util/PlatformUtils.hpp>
using namespace XERCES_CPP_NAMESPACE;
                    ]],
                    [[
XMLPlatformUtils::Initialize();
                    ]]
                )],
                [
                XERCES_LDFLAGS="$xerces_lib_flags"
                xerces_lib_found="yes"
                AC_MSG_RESULT([found])
                ],
                [
                xerces_lib_found="no"
                AC_MSG_RESULT([not found])
                ]
            )
            AC_LANG_POP([C++])
        fi

        CPPFLAGS="$saved_CPPFLAGS"
        LIBS="$saved_LIBS"
    fi

    AC_MSG_CHECKING([for Xerces C++ Parser])

    if test "$run_xerces_test" = "yes"; then
        if test "$xerces_header_found" = "yes" -a "$xerces_lib_found" = "yes"; then
            HAVE_XERCES="yes"
        else
            XERCES_CFLAGS=""
            XERCES_LDFLAGS=""
            HAVE_XERCES="no"
        fi

        AC_MSG_RESULT([$HAVE_XERCES])

        dnl
        dnl Check Xerces version
        dnl
        if test "$HAVE_XERCES" = "yes"; then

            xerces_version_req=ifelse([$1], [], [], [$1])

            if test  -n "$xerces_version_req"; then

                AC_MSG_CHECKING([if Xerces C++ Parser version is >= $xerces_version_req])

                if test -f "$xerces_include_dir2/util/XercesVersion.hpp"; then

                    xerces_major=$(sed -n '/^#define XERCES_VERSION_MAJOR.*$/{s/\([^0-9]*\)\([0-9]*\).*/\2/;P;}' \
		    	$xerces_include_dir2/util/XercesVersion.hpp)
                    xerces_minor=$(sed -n '/^#define XERCES_VERSION_MINOR.*$/{s/\([^0-9]*\)\([0-9]*\).*/\2/;P;}' \
		    	$xerces_include_dir2/util/XercesVersion.hpp)
                    xerces_revision=$(sed -n '/^#define XERCES_VERSION_REVISION.*$/{s/\([^0-9]*\)\([0-9]*\).*/\2/;P;}' \
		    	$xerces_include_dir2/util/XercesVersion.hpp)

                    XERCES_VERSION="$xerces_major.$xerces_minor.$xerces_revision"
                    AC_SUBST([XERCES_VERSION])

                    dnl Decompose required version string and calculate numerical representation
                    xerces_version_req_major=`expr $xerces_version_req : '\([[0-9]]*\)'`
                    xerces_version_req_minor=`expr $xerces_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
                    xerces_version_req_revision=`expr $xerces_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
                    if test "x$xerces_version_req_revision" = "x"; then
                        xerces_version_req_revision="0"
                    fi

                    xerces_version_req_number=`expr $xerces_version_req_major \* 10000 \
                                               \+ $xerces_version_req_minor \* 100 \
                                               \+ $xerces_version_req_revision`

                    dnl Calculate numerical representation of detected version
                    xerces_version_number=`expr $xerces_major \* 10000 \
                                          \+ $xerces_minor \* 100 \
                                           \+ $xerces_revision`

                    xerces_version_check=`expr $xerces_version_number \>\= $xerces_version_req_number`
                    if test "$xerces_version_check" = "1"; then
                        AC_MSG_RESULT([yes])
                    else
                        AC_MSG_RESULT([no])
                        if test "$xerces_requested" = "yes"; then
                            AC_MSG_ERROR([Found Xerces C++ Parser $XERCES_VERSION, which is older than required.])
                        else
                            AC_MSG_WARN([Found Xerces C++ Parser $XERCES_VERSION, which is older than required. Disabling it])
                            XERCES_CFLAGS=""
                            XERCES_LDFLAGS=""
                            HAVE_XERCES="no"
                        fi
                    fi
                else
                    AC_MSG_RESULT([no])
                    AC_MSG_WARN([Missing header XercesVersion.hpp. Unable to determine Xerces version.])
                fi
            fi
        fi

        AC_SUBST([XERCES_CFLAGS])
        AC_SUBST([XERCES_LDFLAGS])

    else
        HAVE_XERCES="no"
        AC_MSG_RESULT([$HAVE_XERCES])

        if test "$xerces_requested" = "yes"; then
            AC_MSG_WARN([Xerces C++ Parser support requested but headers or library not found. Specify valid prefix of Xerces C++ using --with-xerces=@<:@DIR@:>@ or provide include directory and linker flags using --with-xerces-inc and --with-xerces-lib])
        fi
    fi
])
