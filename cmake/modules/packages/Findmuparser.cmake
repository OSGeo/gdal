# * Find muparser
# Find the muparser C++ math expression parser library
#
# muparser::muparser - Imported target to use
# MUPARSER_FOUND - True if muparser # was found.
#
# Original Author: 2019 Rylie Pavlik <rylie.pavlik@collabora.com>
# <rylie@ryliepavlik.com>
#
# Copyright 2019, Collabora, Ltd.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# SPDX-License-Identifier: BSL-1.0

set(MUPARSER_ROOT_DIR
    "${MUPARSER_ROOT_DIR}"
    CACHE PATH "Directory to search for mmuparser")

find_package(PkgConfig QUIET)
pkg_check_modules(PC_MUPARSER QUIET muparser)

find_path(
  MUPARSER_INCLUDE_DIR
  NAMES muParser.h
  PATHS "${MUPARSER_ROOT_DIR}"
  HINTS ${PC_MUPARSER_INCLUDEDIR} ${PC_MUPARSER_INCLUDE_DIRS})
find_library(
  MUPARSER_LIBRARY
  NAMES muparser
  PATHS "${MUPARSER_ROOT_DIR}"
  HINTS ${PC_MUPARSER_LIBDIR} ${PC_MUPARSER_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(muparser DEFAULT_MSG MUPARSER_INCLUDE_DIR
                                  MUPARSER_LIBRARY)

if(MUPARSER_FOUND)
  if(NOT TARGET muparser::muparser)
    add_library(muparser::muparser UNKNOWN IMPORTED)
    set_target_properties(
      muparser::muparser
      PROPERTIES IMPORTED_LOCATION "${MUPARSER_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${MUPARSER_INCLUDE_DIR}")
  endif()
  set(MUPARSER_INCLUDE_DIRS ${MUPARSER_INCLUDE_DIR})
  set(MUPARSER_LIBRARIES ${MUPARSER_LIBRARY})
  mark_as_advanced(MUPARSER_ROOT_DIR)
endif()

mark_as_advanced(MUPARSER_INCLUDE_DIR MUPARSER_LIBRARY)
