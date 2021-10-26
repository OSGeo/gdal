# - Try to find the WebP library
#
# Once done this will define
#
#  WEBP_FOUND - System has libgta
#  WEBP_INCLUDE_DIR - The libgta include directory
#  WEBP_LIBRARIES - The libraries needed to use libgta

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_WEBP QUIET libwebp)
    set(WEBP_VERSION_STRING ${PC_WEBP_VERSION})
endif()

find_path(WEBP_INCLUDE_DIR webp/encode.h HINTS ${PC_WEBP_INCLUDE_DIRS})

find_library(WEBP_LIBRARY NAMES webp libwebp HINTS ${PC_WEBP_LIBRARY_DIRS})

mark_as_advanced(WEBP_INCLUDE_DIR WEBP_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WebP
                                  FOUND_VAR WEBP_FOUND
                                  REQUIRED_VARS WEBP_LIBRARY WEBP_INCLUDE_DIR
                                  VERSION_VAR WEBP_VERSION_STRING
)

if(WEBP_FOUND)
    set(WEBP_LIBRARIES ${WEBP_LIBRARY})
    set(WEBP_INCLUDE_DIRS ${WEBP_INCLUDE_DIR})
    if(NOT TARGET WEBP::WebP)
        add_library(WEBP::WebP UNKNOWN IMPORTED)
        set_target_properties(WEBP::WebP PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDE_DIRS}
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION ${WEBP_LIBRARIES})
    endif()
endif()
