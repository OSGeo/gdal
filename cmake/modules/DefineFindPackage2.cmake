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
    get_property(DEFINE_FIND_PACKAGE_DEFINED GLOBAL PROPERTY define_find_pacakge_pkgname DEFINED)
    if(NOT DEFINE_FIND_PACKAGE_DEFINED)
        define_property(GLOBAL PROPERTY define_find_package_pkgname
                        BRIEF_DOCS "list of package names which is defined with define_find_package2"
                        FULL_DOCS "list of package names which is defined with define_find_package2.")
        define_property(GLOBAL PROPERTY define_find_package_include_file
                        BRIEF_DOCS "list of include file names which is defined with define_find_package2"
                        FULL_DOCS "list of include file names which is defined with define_find_package2."
                                  "An order should be as same as define_find_package_pkgname.")
        define_property(GLOBAL PROPERTY define_find_package_library_name
                        BRIEF_DOCS "list of library  names which is defined with define_find_package2"
                        FULL_DOCS "list of library  names which is defined with define_find_package2."
                                  "An order should be as same as define_find_package_pkgname.")
    endif()
    set_property(GLOBAL APPEND PROPERTY define_find_package_pkgname ${pkgname})
    set_property(GLOBAL APPEND PROPERTY define_find_package_include_file ${include_file})
    set_property(GLOBAL APPEND PROPERTY define_find_package_library_name ${library_name})
endfunction()

function(find_package2 pkgname)
    if(ARGC GREATER 2)
        set(OPTION "${ARGV2}")
        if(OPTION EQUAL "QUIET")
            set(${pkgname}_FIND_QUIETLY TRUE)
        elseif(OPTION EQUAL "REQUIRED")
            set(${pkgname}_FIND_REQUIRED TRUE)
        endif()
    endif()

    get_property(pkgname_list GLOBAL PROPERTY define_find_package_pkgname)
    list(FIND pkgname_list ${pkgname} index)
    if(index GREATER -1)
        get_property(include_file_list GLOBAL PROPERTY define_find_package_include_file)
        list(GET include_file_list ${index} include_file)
        get_property(library_name_list GLOBAL PROPERTY define_find_package_library_name)
        list(GET library_name_list ${index} library_name)
    else()
        ## debug message
        # find_package_message(${pkgname} "Cannot Find definition for ${pkgname} package" "")
        return()
    endif()
    string(TOUPPER ${pkgname} key)
    if(DEFINED ${key}_ROOT)
        set(HINT_PATH "${${key}_ROOT}")
    endif()
    find_path(${pkgname}_INCLUDE_DIR ${include_file} HINTS ${HINT_PATH}/include)
    find_library(${pkgname}_LIBRARY ${library_name} HINTS ${HINT_PATH}/lib)
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
    endif()
    set(${key}_FOUND ${${key}_FOUND} PARENT_SCOPE)
endfunction()
