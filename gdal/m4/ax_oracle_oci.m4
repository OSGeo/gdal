dnl @synopsis AX_LIB_ORACLE_OCI([MINIMUM-VERSION])
dnl
dnl @summary Find Oracle OCI API.
dnl
dnl This macro provides tests of availability of Oracle OCI API of
dnl particular version or newer. This macros checks for Oracle OCI
dnl headers and libraries and defines compilation flags.
dnl
dnl Macro supports following options and their values:
dnl
dnl 1) Single-option usage:
dnl
dnl     --with-oci         -- path to ORACLE_HOME directory
dnl
dnl 2) Two-options usage (both options are required):
dnl
dnl     --with-oci-include -- path to directory with OCI headers
dnl     --with-oci-lib     -- path to directory with OCI libraries
dnl
dnl NOTE: These options described above do not take yes|no values. If
dnl 'yes' value is passed, then WARNING message will be displayed, 'no'
dnl value, as well as the --without-oci-* variations will cause the
dnl macro to not check enything.
dnl
dnl This macro calls:
dnl
dnl   AC_SUBST(ORACLE_OCI_CFLAGS)
dnl   AC_SUBST(ORACLE_OCI_LDFLAGS)
dnl   AC_SUBST(ORACLE_OCI_VERSION)
dnl
dnl And sets:
dnl
dnl   HAVE_ORACLE_OCI
dnl
dnl @category InstalledPackages
dnl @category Cxx
dnl @author Mateusz Loskot <mateusz@loskot.net>
dnl @version 2007-11-30
dnl @license AllPermissive

AC_DEFUN([AX_LIB_ORACLE_OCI],
[
    AC_ARG_WITH([oci],
        AC_HELP_STRING([--with-oci=@<:@ARG@:>@],
            [use Oracle OCI API from given Oracle home (ARG=path); use existing ORACLE_HOME (ARG=yes); disable Oracle OCI support (ARG=no)]
        ),
        [
        if test "$withval" = "yes"; then
            if test -n "$ORACLE_HOME"; then
                oracle_home_dir="$ORACLE_HOME"
            else
                oracle_home_dir=""
            fi
        elif test -d "$withval"; then
            oracle_home_dir="$withval"
        else
            oracle_home_dir=""
        fi
        ],
        [
        if test -n "$ORACLE_HOME"; then
            oracle_home_dir="$ORACLE_HOME"
        else
            oracle_home_dir=""
        fi
        ]
    )

    AC_ARG_WITH([oci-include],
        AC_HELP_STRING([--with-oci-include=@<:@DIR@:>@],
            [use Oracle OCI API headers from given path]
        ),
        [oracle_home_include_dir="$withval"],
        [oracle_home_include_dir=""]
    )
    AC_ARG_WITH([oci-lib],
        AC_HELP_STRING([--with-oci-lib=@<:@DIR@:>@],
            [use Oracle OCI API libraries from given path]
        ),
        [oracle_home_lib_dir="$withval"],
        [oracle_home_lib_dir=""]
    )

    ORACLE_OCI_CFLAGS=""
    ORACLE_OCI_LDFLAGS=""
    ORACLE_OCI_VERSION=""

    dnl
    dnl Collect include/lib paths
    dnl
    want_oracle_but_no_path="no"

    if test -n "$oracle_home_dir"; then

        if test "$oracle_home_dir" != "no" -a "$oracle_home_dir" != "yes"; then
            dnl ORACLE_HOME path provided

            dnl Primary path to OCI headers, available in Oracle>=10
            if test -d "$oracle_home_dir/rdbms/public"; then
                oracle_include_dir="$oracle_home_dir/rdbms/public"
            fi

            dnl Secondary path to OCI headers used by older versions
            if test -d "$oracle_home_dir/rdbms/demo"; then
                oracle_include_dir="$oracle_home_dir/rdbms/demo"
            fi

            dnl Another path to OCI headers used by instant client
            if test -d "$oracle_home_dir/sdk/include"; then
                oracle_include_dir="$oracle_home_dir/sdk/include"
            fi

            dnl Library path
            oracle_lib_dir="$oracle_home_dir/lib"

            dnl Library path for instant client
            oracle_lib_dir2="$oracle_home_dir"

        elif test "$oracle_home_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi

    elif test -n "$oracle_home_include_dir" -o -n "$oracle_home_lib_dir"; then

        if test "$oracle_home_include_dir" != "no" -a "$oracle_home_include_dir" != "yes"; then
            oracle_include_dir="$oracle_home_include_dir"
        elif test "$oracle_home_include_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi

        if test "$oracle_home_lib_dir" != "no" -a "$oracle_home_lib_dir" != "yes"; then
            oracle_lib_dir="$oracle_home_lib_dir"
        elif test "$oracle_home_lib_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi
    fi

    if test "$want_oracle_but_no_path" = "yes"; then
        AC_MSG_WARN([Oracle support is requested but no Oracle paths have been provided. \
Please, locate Oracle directories using --with-oci or \
--with-oci-include and --with-oci-lib options.])
    fi

    dnl
    dnl Check OCI files
    dnl
    if test -n "$oracle_include_dir" -a -n "$oracle_lib_dir" -o -n "$oracle_lib_dir2"; then

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS -I$oracle_include_dir"

        dnl Depending on later Oracle version detection,
        dnl -lnnz10 flag might be removed for older Oracle < 10.x
        if test -n "$oracle_include_dir" ; then
          oci_ldflags="-L$oracle_lib_dir -lclntsh"
        else
          oci_ldflags="-L$oracle_lib_dir -L$oracle_lib_dir2 -lclntsh"
        fi

        saved_LIBS="$LIBS"
        LIBS="$LIBS $oci_ldflags"

        dnl
        dnl Check OCI headers
        dnl
        if test -f "$oracle_include_dir/oci.h"; then
            ACTIVE_INCLUDE_DIR="$oracle_include_dir"
        fi
        AC_MSG_CHECKING([for Oracle OCI headers in $ACTIVE_INCLUDE_DIR])

        AC_LANG_PUSH(C++)
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[@%:@include <oci.h>]],
                [[
#if defined(OCI_MAJOR_VERSION)
#if OCI_MAJOR_VERSION == 10 && OCI_MINOR_VERSION == 2
// Oracle 10.2 detected
#endif
#elif defined(OCI_V7_SYNTAX)
// OK, older Oracle detected
// TODO - mloskot: find better macro to check for older versions;
#else
#  error Oracle oci.h header not found
#endif
                ]]
            )],
            [
            ORACLE_OCI_CFLAGS="-I$oracle_include_dir"

            oci_header_found="yes"
            AC_MSG_RESULT([yes])
            ],
            [
            oci_header_found="no"
            AC_MSG_RESULT([ header not found])
            ]
        )
        AC_LANG_POP([C++])

        dnl
        dnl Check OCI libraries
        dnl
        if test "$oci_header_found" = "yes"; then

            AC_MSG_CHECKING([for Oracle OCI libraries in $oracle_lib_dir])

            AC_LANG_PUSH(C++)
            AC_LINK_IFELSE([
                AC_LANG_PROGRAM([[@%:@include <oci.h>]],
                    [[
OCIEnv* envh = 0;
OCIEnvCreate(&envh, OCI_DEFAULT, 0, 0, 0, 0, 0, 0);
if (envh) OCIHandleFree(envh, OCI_HTYPE_ENV);
                    ]]
                )],
                [
                ORACLE_OCI_LDFLAGS="$oci_ldflags"
                oci_lib_found="yes"
                AC_MSG_RESULT([yes])
                ],
                [
                oci_lib_found="no"
                AC_MSG_RESULT([library not found])
                ]
            )
            AC_LANG_POP([C++])
        fi

        CPPFLAGS="$saved_CPPFLAGS"
        LIBS="$saved_LIBS"
    fi

    dnl
    dnl Check required version of Oracle is available
    dnl
    oracle_version_req=ifelse([$1], [], [], [$1])

    if test "$oci_header_found" = "yes" -a "$oci_lib_found" = "yes" -a \
        -n "$oracle_version_req"; then

        if test -f "$oracle_include_dir/oci.h"; then
            ACTIVE_INCLUDE_DIR="$oracle_include_dir"
        fi

        oracle_version_major=$(sed -n '/^#define OCI_MAJOR_VERSION.*$/{s/\([^0-9]*\)\([0-9]*\).*/\2/;P;}' \
		$ACTIVE_INCLUDE_DIR/oci.h)
        oracle_version_minor=$(sed -n '/^#define OCI_MINOR_VERSION.*$/{s/\([^0-9]*\)\([0-9]*\).*/\2/;P;}' \
		$ACTIVE_INCLUDE_DIR/oci.h)

        AC_MSG_CHECKING([if Oracle OCI version is >= $oracle_version_req ])

        if test -n "$oracle_version_major" -a -n $"oracle_version_minor"; then

            ORACLE_OCI_VERSION="$oracle_version_major.$oracle_version_minor"

            dnl Decompose required version string of Oracle
            dnl and calculate its number representation
            oracle_version_req_major=`expr $oracle_version_req : '\([[0-9]]*\)'`
            oracle_version_req_minor=`expr $oracle_version_req : '[[0-9]]*\.\([[0-9]]*\)'`

            oracle_version_req_number=`expr $oracle_version_req_major \* 1000000 \
                                       \+ $oracle_version_req_minor \* 1000`

            dnl Calculate its number representation
            oracle_version_number=`expr $oracle_version_major \* 1000000 \
                                  \+ $oracle_version_minor \* 1000`

            oracle_version_check=`expr $oracle_version_number \>\= $oracle_version_req_number`
            if test "$oracle_version_check" = "1"; then

                oracle_version_checked="yes"
                AC_MSG_RESULT([yes])

                dnl Add -lnnz10 flag to Oracle >= 10.x
                AC_MSG_CHECKING([for Oracle version >= 10.x to use -lnnz10 flag])
                oracle_nnz10_check=`expr $oracle_version_number \>\= 10 \* 1000000`
                if test "$oracle_nnz10_check" = "1"; then
                    ORACLE_OCI_LDFLAGS="$ORACLE_OCI_LDFLAGS -lnnz$oracle_version_major"
                    AC_MSG_RESULT([yes])
                else
                    AC_MSG_RESULT([no])
                fi
            else
                oracle_version_checked="no"
                AC_MSG_RESULT([no])
                AC_MSG_ERROR([Oracle $ORACLE_OCI_VERSION found, but required version is $oracle_version_req])
            fi
        else
            ORACLE_OCI_VERSION="UNKNOWN"
            AC_MSG_RESULT([no])
            AC_MSG_WARN([Oracle version unknown, probably OCI older than 10.2 is available])
        fi
    fi

    AC_MSG_CHECKING([if Oracle support is enabled])

    if test "$oci_header_found" = "yes" -a "$oci_lib_found" = "yes"; then

        AC_SUBST([ORACLE_OCI_VERSION])
        AC_SUBST([ORACLE_OCI_CFLAGS])
        AC_SUBST([ORACLE_OCI_LDFLAGS])

        HAVE_ORACLE_OCI="yes"
    else
        HAVE_ORACLE_OCI="no"
    fi

    AC_MSG_RESULT([$HAVE_ORACLE_OCI])
])
