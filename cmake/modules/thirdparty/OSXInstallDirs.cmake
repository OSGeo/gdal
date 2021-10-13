# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# OSXInstallDirs
# --------------
#
# Define installation directories for Mac OSX/iOS
# it is off from OSXInstallDirs in cmake 3.12.0
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# Inclusion of this module defines the following variables:
#
# ``CMAKE_INSTALL_<dir>``
#
#   Destination for files of a given type.  This value may be passed to
#   the ``DESTINATION`` options of :command:`install` commands for the
#   corresponding file type.
#
# where ``<dir>`` is one of:
#
# ``BINDIR``
#   user executables (``bin``)
# ``SBINDIR``
#   system admin executables (``sbin``)
# ``LIBEXECDIR``
#   program executables (``libexec``)
# ``SYSCONFDIR``
#   read-only single-machine data (``etc``)
# ``SHAREDSTATEDIR``
#   modifiable architecture-independent data (``com``)
# ``LOCALSTATEDIR``
#   modifiable single-machine data (``var``)
# ``RUNSTATEDIR``
#   run-time variable data (``LOCALSTATEDIR/run``)
# ``LIBDIR``
#   object code libraries (``Libraries/Frameworks``
# ``INCLUDEDIR``
#   C header files (``include``)
# ``DATAROOTDIR``
#   read-only architecture-independent data root (``share``)
# ``DATADIR``
#   read-only architecture-independent data (``DATAROOTDIR``)
# ``INFODIR``
#   info documentation (``DATAROOTDIR/info``)
# ``LOCALEDIR``
#   locale-dependent data (``DATAROOTDIR/locale``)
# ``MANDIR``
#   man documentation (``DATAROOTDIR/man``)
# ``DOCDIR``
#   documentation root (``DATAROOTDIR/doc/PROJECT_NAME``)
#
# If the includer does not define a value the above-shown default will be
# used and the value will appear in the cache for editing by the user.
#

cmake_policy(PUSH)
cmake_policy(SET CMP0054 NEW) # if() quoted variables not dereferenced

# Convert a cache variable to PATH type

macro(_OSXInstallDirs_cache_convert_to_path var description)
  get_property(_OSXInstallDirs_cache_type CACHE ${var} PROPERTY TYPE)
  if(_OSXInstallDirs_cache_type STREQUAL "UNINITIALIZED")
    file(TO_CMAKE_PATH "${${var}}" _OSXInstallDirs_cmakepath)
    set_property(CACHE ${var} PROPERTY TYPE PATH)
    set_property(CACHE ${var} PROPERTY VALUE "${_OSXInstallDirs_cmakepath}")
    set_property(CACHE ${var} PROPERTY HELPSTRING "${description}")
    unset(_OSXInstallDirs_cmakepath)
  endif()
  unset(_OSXInstallDirs_cache_type)
endmacro()

# Create a cache variable with default for a path.
macro(_OSXInstallDirs_cache_path var default description)
  if(NOT DEFINED ${var})
    set(${var} "${default}" CACHE PATH "${description}")
  endif()
  _OSXInstallDirs_cache_convert_to_path("${var}" "${description}")
endmacro()

# Create a cache variable with not default for a path, with a fallback
# when unset; used for entries slaved to other entries such as
# DATAROOTDIR.
macro(_OSXInstallDirs_cache_path_fallback var default description)
  if(NOT ${var})
    set(${var} "" CACHE PATH "${description}")
    set(${var} "${default}")
  endif()
  _OSXInstallDirs_cache_convert_to_path("${var}" "${description}")
endmacro()

# Installation directories
#

_OSXInstallDirs_cache_path(CMAKE_INSTALL_BINDIR "bin"
  "User executables (bin)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_SBINDIR "sbin"
  "System admin executables (sbin)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_LIBEXECDIR "libexec"
  "Program executables (libexec)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_SYSCONFDIR "etc"
  "Read-only single-machine data (etc)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_SHAREDSTATEDIR "com"
  "Modifiable architecture-independent data (com)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_LOCALSTATEDIR "var"
  "Modifiable single-machine data (var)")

# We check if the variable was manually set and not cached, in order to
# allow projects to set the values as normal variables before including
# OSXInstallDirs to avoid having the entries cached or user-editable. It
# replaces the "if(NOT DEFINED CMAKE_INSTALL_XXX)" checks in all the
# other cases.
# If CMAKE_INSTALL_LIBDIR is defined, if _libdir_set is false, then the
# variable is a normal one, otherwise it is a cache one.
get_property(_libdir_set CACHE CMAKE_INSTALL_LIBDIR PROPERTY TYPE SET)
if(NOT DEFINED CMAKE_INSTALL_LIBDIR OR (_libdir_set
    AND DEFINED _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX
    AND NOT "${_OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX}" STREQUAL "${CMAKE_INSTALL_PREFIX}"))
  # If CMAKE_INSTALL_LIBDIR is not defined, it is always executed.
  # Otherwise:
  #  * if _libdir_set is false it is not executed (meaning that it is
  #    not a cache variable)
  #  * if _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX is not defined it is
  #    not executed
  #  * if _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX and
  #    CMAKE_INSTALL_PREFIX are the same string it is not executed.
  #    _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX is updated after the
  #    execution, of this part of code, therefore at the next inclusion
  #    of the file, CMAKE_INSTALL_LIBDIR is defined, and the 2 strings
  #    are equal, meaning that the if is not executed the code the
  #    second time.

  set(_LIBDIR_DEFAULT "Library/Frameworks")
  if(DEFINED _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
    set(__LAST_LIBDIR_DEFAULT "Library/Frameworks")
    # __LAST_LIBDIR_DEFAULT is the default value that we compute from
    # _OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX, not a cache entry for
    # the value that was last used as the default.
    # This value is used to figure out whether the user changed the
    # CMAKE_INSTALL_LIBDIR value manually, or if the value was the
    # default one. When CMAKE_INSTALL_PREFIX changes, the value is
    # updated to the new default, unless the user explicitly changed it.
  endif()
  if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "${_LIBDIR_DEFAULT}" CACHE PATH "Object code libraries (${_LIBDIR_DEFAULT})")
  elseif(DEFINED __LAST_LIBDIR_DEFAULT
      AND "${__LAST_LIBDIR_DEFAULT}" STREQUAL "${CMAKE_INSTALL_LIBDIR}")
    set_property(CACHE CMAKE_INSTALL_LIBDIR PROPERTY VALUE "${_LIBDIR_DEFAULT}")
  endif()
endif()
_OSXInstallDirs_cache_convert_to_path(CMAKE_INSTALL_LIBDIR "Object code libraries (lib)")

# Save for next run
set(_OSXInstallDirs_LAST_CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "CMAKE_INSTALL_PREFIX during last run")
unset(_libdir_set)
unset(__LAST_LIBDIR_DEFAULT)

_OSXInstallDirs_cache_path(CMAKE_INSTALL_INCLUDEDIR "include"
  "C header files (include)")
_OSXInstallDirs_cache_path(CMAKE_INSTALL_DATAROOTDIR "share"
  "Read-only architecture-independent data root (share)")

#-----------------------------------------------------------------------------
# Values whose defaults are relative to DATAROOTDIR.  Store empty values in
# the cache and store the defaults in local variables if the cache values are
# not set explicitly.  This auto-updates the defaults as DATAROOTDIR changes.

_OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}"
  "Read-only architecture-independent data (DATAROOTDIR)")

if(CMAKE_SYSTEM_NAME MATCHES "^(.*BSD|DragonFly)$")
  _OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_INFODIR "info"
    "Info documentation (info)")
  _OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_MANDIR "man"
    "Man documentation (man)")
else()
  _OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_INFODIR "${CMAKE_INSTALL_DATAROOTDIR}/info"
    "Info documentation (DATAROOTDIR/info)")
  _OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_MANDIR "${CMAKE_INSTALL_DATAROOTDIR}/man"
    "Man documentation (DATAROOTDIR/man)")
endif()

_OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_LOCALEDIR "${CMAKE_INSTALL_DATAROOTDIR}/locale"
  "Locale-dependent data (DATAROOTDIR/locale)")
_OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}"
  "Documentation root (DATAROOTDIR/doc/PROJECT_NAME)")

_OSXInstallDirs_cache_path_fallback(CMAKE_INSTALL_RUNSTATEDIR "${CMAKE_INSTALL_LOCALSTATEDIR}/run"
  "Run-time variable data (LOCALSTATEDIR/run)")

#-----------------------------------------------------------------------------

mark_as_advanced(
  CMAKE_INSTALL_BINDIR
  CMAKE_INSTALL_SBINDIR
  CMAKE_INSTALL_LIBEXECDIR
  CMAKE_INSTALL_SYSCONFDIR
  CMAKE_INSTALL_SHAREDSTATEDIR
  CMAKE_INSTALL_LOCALSTATEDIR
  CMAKE_INSTALL_RUNSTATEDIR
  CMAKE_INSTALL_LIBDIR
  CMAKE_INSTALL_INCLUDEDIR
  CMAKE_INSTALL_DATAROOTDIR
  CMAKE_INSTALL_DATADIR
  CMAKE_INSTALL_INFODIR
  CMAKE_INSTALL_LOCALEDIR
  CMAKE_INSTALL_MANDIR
  CMAKE_INSTALL_DOCDIR
  )

cmake_policy(POP)
