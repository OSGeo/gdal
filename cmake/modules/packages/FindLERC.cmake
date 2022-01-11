# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLERC
--------

The following vars are set if LERC is found.

.. variable:: LERC_FOUND

  True if found, otherwise all other vars are undefined

.. variable:: LERC_INCLUDE_DIR

  The include dir for main *.h files

.. variable:: LERC_LIBRARY

  The library file

#]=======================================================================]


include(FindPackageHandleStandardArgs)

find_path(LERC_INCLUDE_DIR NAMES Lerc_c_api.h)
find_library(LERC_LIBRARY NAMES Lerc lerc)

mark_as_advanced(LERC_INCLUDE_DIR LERC_LIBRARY)
find_package_handle_standard_args(LERC
                                  FOUND_VAR LERC_FOUND
                                  REQUIRED_VARS LERC_LIBRARY LERC_INCLUDE_DIR)

if(LERC_FOUND)
    set(LERC_LIBRARIES ${LERC_LIBRARY})
    set(LERC_INCLUDE_DIRS ${LERC_INCLUDE_DIR})

    if(NOT TARGET LERC::LERC)
        add_library(LERC::LERC UNKNOWN IMPORTED)
        set_target_properties(LERC::LERC PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${LERC_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${LERC_LIBRARY}")
    endif()
endif()
