# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file COPYING-CMAKE-SCRIPTS or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPDFium
-----------

Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>

Find the PDFium headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``PDFIUM::PDFium``, if
PDFium has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``PDFIUM_FOUND``
  True if PDFium found.

``PDFIUM_INCLUDE_DIRS``
  where to find header files.

``PDFIUM_LIBRARIES``
  List of PDFium libraries to link.

``PDFIUM_VERSION_STRING``
  Version of PDFium library.

#]=======================================================================]

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_PDFIUM QUIET pdfium)
endif()

find_path(PDFIUM_INCLUDE_DIR
          NAMES public/fpdfview.h
          HINTS ${PC_PDFIUM_INCLUDE_DIRS}
          PATH_SUFFIXES pdfium)

find_library(PDFIUM_LIBRARY
             NAMES pdfium libpdfium
             HINTS ${PC_PDFIUM_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PDFium
                                  FOUND_VAR PDFIUM_FOUND
                                  REQUIRED_VARS PDFIUM_LIBRARY PDFIUM_INCLUDE_DIR)

if(PDFIUM_FOUND)
  set(PDFIUM_INCLUDE_DIRS ${PDFIUM_INCLUDE_DIR})
  set(PDFIUM_LIBRARIES ${PDFIUM_LIBRARY})
  if(NOT TARGET PDFIUM:PDFium)
    add_library(PDFIUM::PDFium UNKNOWN IMPORTED)
    set_target_properties(PDFIUM::PDFium PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES ${PDFIUM_INCLUDE_DIR}
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION ${PDFIUM_LIBRARY})
  endif()
endif()
