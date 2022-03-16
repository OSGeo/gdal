# Distributed under OSI approved BSD-3 style License.  See accompanying
# file Copyright.txt.

#[=======================================================================[.rst:
DefineFindPackage2
------------------

Define `find_package2()` which has similar functionality as `find_package`
and `define_find_package2()` that declare package properties to find.

`find_package2(<pkgname>)` returns `<Upper-case pkgname>_FOUND`,
`<Upper-case pkgname>_LIBRARIES` and `<Upper-case pkgname>_INCLUDE_DIRS'
variables. And also set imported Target `<Upper-case pkgname>::<pkgname>`.

`find_package2()` accept two options, `QUIET` and `REQUIRED`.
There is no support for `VERSION` and `COMPONENTS`.

This script only support package that has only one library name and
single directory for include files. If your target is supposed not as
the condition, it is a time to make a actual custom Find*.cmake file.

#]=======================================================================]

if(__INCLUDED_DEFINE_FIND_PACKAGE2__)
    return()
endif()
set(__INCLUDED_DEFINE_FIND_PACKAGE2__ TRUE)
include(FindPackageMessage)
include(FindPackageHandleStandardArgs)

function(define_find_package2 pkgname include_file library_name)
    set(_options)
    set(_oneValueArgs PKGCONFIG_NAME FIND_PATH_SUFFIX)
    set(_multiValueArgs)
    cmake_parse_arguments(_DFP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    get_property(DEFINE_FIND_PACKAGE_DEFINED GLOBAL PROPERTY define_find_package_pkgname DEFINED)
    if(NOT DEFINE_FIND_PACKAGE_DEFINED)
        define_property(GLOBAL PROPERTY define_find_package_pkgname
                        BRIEF_DOCS "list of package names which is defined with define_find_package2"
                        FULL_DOCS "list of package names which is defined with define_find_package2.")
        define_property(GLOBAL PROPERTY define_find_package_include_file
                        BRIEF_DOCS "list of include file names which is defined with define_find_package2"
                        FULL_DOCS "list of include file names which is defined with define_find_package2."
                                  "The order should be as same as define_find_package_pkgname.")
        define_property(GLOBAL PROPERTY define_find_package_library_name
                        BRIEF_DOCS "list of library names which is defined with define_find_package2"
                        FULL_DOCS "list of library names which is defined with define_find_package2."
                                  "The order should be as same as define_find_package_pkgname.")
        define_property(GLOBAL PROPERTY define_find_package_pkgconfig_name
                        BRIEF_DOCS "list of pkg-config names which is defined with define_find_package2"
                        FULL_DOCS "list of pkg-config  names which is defined with define_find_package2."
                                  "The order should be as same as define_find_package_pkgname.")
        define_property(GLOBAL PROPERTY define_find_package_find_path_suffix
                        BRIEF_DOCS "list of FIND_PATH_SUFFIX which is defined with define_find_package2"
                        FULL_DOCS "list of FIND_PATH_SUFFIX which is defined with define_find_package2."
                                  "The order should be as same as define_find_package_pkgname.")
    endif()
    set_property(GLOBAL APPEND PROPERTY define_find_package_pkgname ${pkgname})
    set_property(GLOBAL APPEND PROPERTY define_find_package_include_file ${include_file})
    set_property(GLOBAL APPEND PROPERTY define_find_package_library_name ${library_name})
    if(DEFINED _DFP_PKGCONFIG_NAME)
        set_property(GLOBAL APPEND PROPERTY define_find_package_pkgconfig_name "${_DFP_PKGCONFIG_NAME}")
    else()
        set_property(GLOBAL APPEND PROPERTY define_find_package_pkgconfig_name "_unset_")
    endif()
    if(DEFINED _DFP_FIND_PATH_SUFFIX)
        set_property(GLOBAL APPEND PROPERTY define_find_package_find_path_suffix "${_DFP_FIND_PATH_SUFFIX}")
    else()
        set_property(GLOBAL APPEND PROPERTY define_find_package_find_path_suffix "_unset_")
    endif()
endfunction()

function(find_package2 pkgname)
    set(_options QUIET REQUIRED)
    set(_oneValueArgs OUT_DEPENDENCY)
    set(_multiValueArgs)
    cmake_parse_arguments(arg "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if(arg_QUIET)
        set(${pkgname}_FIND_QUIETLY TRUE)
    endif()
    if(arg_REQUIRED)
        set(${pkgname}_FIND_REQUIRED TRUE)
    endif()

    get_property(pkgname_list GLOBAL PROPERTY define_find_package_pkgname)
    list(FIND pkgname_list ${pkgname} index)
    if(index GREATER -1)
        get_property(include_file_list GLOBAL PROPERTY define_find_package_include_file)
        list(GET include_file_list ${index} include_file)
        get_property(library_name_list GLOBAL PROPERTY define_find_package_library_name)
        list(GET library_name_list ${index} library_name)
        get_property(pkgconfig_name_list GLOBAL PROPERTY define_find_package_pkgconfig_name)
        list(GET pkgconfig_name_list ${index} pkgconfig_name)
        get_property(find_path_suffix_list GLOBAL PROPERTY define_find_package_find_path_suffix)
        list(GET find_path_suffix_list ${index} find_path_suffix)
    else()
        ## debug message
        # find_package_message(${pkgname} "Cannot Find definition for ${pkgname} package" "")
        return()
    endif()

    string(TOUPPER ${pkgname} key)

    if(NOT pkgconfig_name STREQUAL "_unset_")
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PC_${key} QUIET ${pkgconfig_name})
            if(DEFINED PC_${key}_VERSION)
                set(${key}_VERSION_STRING ${PC_${key}_VERSION})
            endif()
            if(DEFINED PC_${key}_INCLUDE_DIRS)
                set(_pc_include_dirs "${PC_${key}_INCLUDE_DIRS}")
            endif()
            if(DEFINED PC_${key}_LIBRARY_DIRS)
                set(_pc_library_dirs "${PC_${key}_LIBRARY_DIRS}")
            endif()
        endif()
    endif()

    if(NOT find_path_suffix STREQUAL "_unset_")
        set(_find_path_suffix "${find_path_suffix}")
    endif()

    if(DEFINED ${key}_ROOT)
        set(HINT_PATH "${${key}_ROOT}")
    endif()
    find_path(${pkgname}_INCLUDE_DIR ${include_file}
              HINTS ${HINT_PATH}/include ${_pc_include_dirs}
              PATH_SUFFIXES ${_find_path_suffix})
    find_library(${pkgname}_LIBRARY ${library_name} HINTS ${HINT_PATH}/lib ${_pc_library_dirs})
    find_package_handle_standard_args(${pkgname}
                                      FOUND_VAR ${key}_FOUND
                                      REQUIRED_VARS ${pkgname}_LIBRARY ${pkgname}_INCLUDE_DIR)
    mark_as_advanced(${pkgname}_INCLUDE_DIR ${pkgname}_LIBRARY)
    if(${key}_FOUND)
        set(${key}_INCLUDE_DIRS "${${pkgname}_INCLUDE_DIR}" PARENT_SCOPE)
        set(${key}_LIBRARIES "${${pkgname}_LIBRARY}" PARENT_SCOPE)
        if(NOT TARGET ${key}::${pkgname})
            add_library(${key}::${pkgname} UNKNOWN IMPORTED)
            set_target_properties(${key}::${pkgname} PROPERTIES
                                  INTERFACE_INCLUDE_DIRECTORIES "${${pkgname}_INCLUDE_DIR}"
                                  IMPORTED_LINK_LANGUAGES C
                                  IMPORTED_LOCATION "${${pkgname}_LIBRARY}")
        endif()
        if(arg_OUT_DEPENDENCY)
            set(output "define_find_package2(${pkgname} \"${include_file}\" \"${library_name}\"")
            if(NOT pkgconfig_name STREQUAL "_unset_")
                string(APPEND output " PKGCONFIG_NAME \"${pkgconfig_name}\"")
            endif()
            if(NOT find_path_suffix STREQUAL "_unset_")
                string(APPEND output " FIND_PATH_SUFFIX \"${find_path_suffix}\"")
            endif()
            string(APPEND output ")\nfind_package2(${pkgname} REQUIRED)\n")
            set(${arg_OUT_DEPENDENCY} ${output} PARENT_SCOPE)
        endif()
    endif()
    set(${key}_FOUND ${${key}_FOUND} PARENT_SCOPE)
endfunction()
