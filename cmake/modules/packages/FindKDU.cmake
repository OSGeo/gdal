# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindKDU
# -----------
#
# CMake module to search for KAKADU library
#
# Copyright (C) 2017-2018, Hiroshi Miura
#
# If it's found it sets KDU_FOUND to TRUE
# and following variables are set:
#    KDU_INCLUDE_DIRS
#    KDU_LIBRARIES
# and it defines the KDU::KDU target

macro(find_kdu_libs_from_makefiles)
    # first argument = platform
    # second argument = library extension (without .)
    file(READ "${KDU_ROOT}/coresys/make/Makefile-${ARGV0}" CORESYS_MAKEFILE_CONTENTS)
    string(REGEX REPLACE ".*SHARED_LIB_NAME[ \t]*=[ \t]*([^.]*)(\.${ARGV1}).*" "\\1\\2" KDU_SHARED_LIB_NAME ${CORESYS_MAKEFILE_CONTENTS})
    file(READ "${KDU_ROOT}/managed/make/Makefile-${ARGV0}" MANAGED_MAKEFILE_CONTENTS)
    string(REGEX REPLACE ".*AUX_SHARED_LIB_NAME[ \t]*=[ \t]*([^.]*)(\.${ARGV1}).*" "\\1\\2" KDU_AUX_SHARED_LIB_NAME ${MANAGED_MAKEFILE_CONTENTS})

    string(REGEX REPLACE "libkdu_v([0-9])([0-9A-F])R\.${ARGV1}" "\\1" KDU_MAJOR_VERSION "${KDU_SHARED_LIB_NAME}")
    string(REGEX REPLACE "libkdu_v([0-9])([0-9A-F])R\.${ARGV1}" "\\2" KDU_MINOR_VERSION "${KDU_SHARED_LIB_NAME}")
    if( KDU_MINOR_VERSION STREQUAL "A" )
        set(KDU_MINOR_VERSION 10)
    endif()
    set(KDU_VERSION_VAR "${KDU_MAJOR_VERSION}.${KDU_MINOR_VERSION}")

    find_library(KDU_LIBRARY ${KDU_SHARED_LIB_NAME}
                 PATH ${KDU_ROOT}/lib/${ARGV0})
    find_library(KDU_AUX_LIBRARY ${KDU_AUX_SHARED_LIB_NAME}
                 PATH ${KDU_ROOT}/lib/${ARGV0})
endmacro()

set(KDU_VERSION_VAR "")
if(KDU_ROOT AND EXISTS "${KDU_ROOT}/coresys")
    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        find_kdu_libs_from_makefiles("Linux-x86-64-gcc" "so")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        find_kdu_libs_from_makefiles("Linux-arm-64-gcc" "so")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        find_kdu_libs_from_makefiles("Mac-x86-64-gcc" "so")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        find_kdu_libs_from_makefiles("Mac-arm-64-gcc" "so")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows" AND MINGW)
        find_kdu_libs_from_makefiles("Mingw-x86-64-gcc" "dll")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows" AND MSVC)
        file(READ "${KDU_ROOT}/coresys/coresys_2019.vcxproj" CORESYS_MAKEFILE_CONTENTS)
        string(REGEX REPLACE [[.*<ImportLibrary>([^l]*)(lib_x64\\)([^R]*R)(\.lib).*]] "\\1\\2\\3\\4" KDU_SHARED_LIB_NAME ${CORESYS_MAKEFILE_CONTENTS})
        file(READ "${KDU_ROOT}/managed/kdu_aux/kdu_aux_2019.vcxproj" MANAGED_MAKEFILE_CONTENTS)
        string(REGEX REPLACE [[.*<ImportLibrary>([^l]*)(lib_x64\\)([^R]*R)(\.lib).*]] "\\1\\2\\3\\4" KDU_AUX_SHARED_LIB_NAME ${MANAGED_MAKEFILE_CONTENTS})

        find_library(KDU_LIBRARY ${KDU_SHARED_LIB_NAME}
                     PATH ${KDU_ROOT})

        find_library(KDU_AUX_LIBRARY ${KDU_AUX_SHARED_LIB_NAME}
                     PATH ${KDU_ROOT})
    endif()
endif()

find_path(KDU_INCLUDE_DIR coresys/common/kdu_elementary.h
          PATH ${KDU_ROOT})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KDU
                                  REQUIRED_VARS KDU_INCLUDE_DIR KDU_LIBRARY KDU_AUX_LIBRARY
                                  VERSION_VAR KDU_VERSION_VAR)
mark_as_advanced(KDU_INCLUDE_DIR KDU_LIBRARY KDU_AUX_LIBRARY)

if(KDU_FOUND)
    set(KDU_INCLUDE_DIRS ${KDU_INCLUDE_DIR})
    set(KDU_LIBRARIES ${KDU_LIBRARY} ${KDU_AUX_LIBRARY})
    if(NOT TARGET KDU::KDU)
        add_library(KDU::KDU_MAIN UNKNOWN IMPORTED)
        set_target_properties(KDU::KDU_MAIN PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${KDU_INCLUDE_DIR}/coresys/common"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${KDU_LIBRARY}")

        add_library(KDU::KDU_AUX UNKNOWN IMPORTED)
        set_target_properties(KDU::KDU_AUX PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${KDU_INCLUDE_DIR}/apps/compressed_io;${KDU_INCLUDE_DIR}/apps/jp2;${KDU_INCLUDE_DIR}/apps/image;${KDU_INCLUDE_DIR}/apps/args;${KDU_INCLUDE_DIR}/apps/support;${KDU_INCLUDE_DIR}/apps/kdu_compress"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${KDU_AUX_LIBRARY}")

        add_library(KDU::KDU INTERFACE IMPORTED)
        set_target_properties(KDU::KDU PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${KDU_INCLUDE_DIR}/coresys/common;${KDU_INCLUDE_DIR}/apps/compressed_io;${KDU_INCLUDE_DIR}/apps/jp2;${KDU_INCLUDE_DIR}/apps/image;${KDU_INCLUDE_DIR}/apps/args;${KDU_INCLUDE_DIR}/apps/support;${KDU_INCLUDE_DIR}/apps/kdu_compress"
                              INTERFACE_LINK_LIBRARIES "KDU::KDU_MAIN;KDU::KDU_AUX")
        if( DEFINED KDU_MAJOR_VERSION AND ${KDU_MAJOR_VERSION}.${KDU_MINOR_VERSION} VERSION_LESS 7.5 )
            set_property(TARGET KDU::KDU
                         APPEND PROPERTY
                         INTERFACE_COMPILE_DEFINITIONS "KDU_MAJOR_VERSION=${KDU_MAJOR_VERSION};KDU_MINOR_VERSION=${KDU_MINOR_VERSION}")
        endif()
    endif()
endif()
