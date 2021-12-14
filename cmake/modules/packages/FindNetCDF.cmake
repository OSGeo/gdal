# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindNetCDF
# ---------
#
# Find the native NetCDF includes and library
#
#  NETCDF_INCLUDE_DIR  - user modifiable choice of where netcdf headers are
#  NETCDF_LIBRARY      - user modifiable choice of where netcdf libraries are
#
# Your package can require certain interfaces to be FOUND by setting these
#
#  NETCDF_CXX         - require the C++ interface and link the C++ library
#  NETCDF_F77         - require the F77 interface and link the fortran library
#  NETCDF_F90         - require the F90 interface and link the fortran library
#
# Or equivalently by calling FindNetCDF with a COMPONENTS argument containing one or
# more of "CXX;F77;F90".
#
# When interfaces are requested the user has access to interface specific hints:
#
#  NETCDF_${LANG}_INCLUDE_DIR - where to search for interface header files
#  NETCDF_${LANG}_LIBRARY     - where to search for interface libraries
#
# This module returns these variables for the rest of the project to use.
#
#  NETCDF_FOUND          - True if NetCDF found including required interfaces (see below)
#  NETCDF_LIBRARIES      - All netcdf related libraries.
#  NETCDF_INCLUDE_DIRS   - All directories to include.
#  NETCDF_HAS_INTERFACES - Whether requested interfaces were found or not.
#  NETCDF_${LANG}_INCLUDE_DIRS/NETCDF_${LANG}_LIBRARIES - C/C++/F70/F90 only interface
#
# Normal usage would be:
#  set (NETCDF_F90 "YES")
#  find_package (NetCDF REQUIRED)
#  target_link_libraries (uses_everthing ${NETCDF_LIBRARIES})
#  target_link_libraries (only_uses_f90 ${NETCDF_F90_LIBRARIES})
#
# It detect NetCDF configurations and set configurations in variable
#
# NETCDF_HAS_NC2
# NETCDF_HAS_NC4
# NETCDF_HAS_HDF4
# NETCDF_HAS_HDF5
# NETCDF_HAS_DAP
# NETCDF_HAS_MEM
#

#search starting from user editable cache var
if (NETCDF_INCLUDE_DIR AND NETCDF_LIBRARY)
    # Already in cache, be silent
    set (NETCDF_FIND_QUIETLY TRUE)
endif ()
find_program(NC_CONFIG NAMES nc-config DOC "NetCDF config command")
mark_as_advanced(NC_CONFIG)

if(NC_CONFIG)
    execute_process(COMMAND ${NC_CONFIG} --includedir
                    RESULT_VARIABLE nc_res
                    OUTPUT_VARIABLE NETCDF_INCLUDE_PATH
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${NC_CONFIG} --prefix
                    RESULT_VARIABLE nc_res
                    OUTPUT_VARIABLE NETCDF_PREFIX_DIR
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

find_path(NETCDF_INCLUDE_DIR netcdf.h
          PATHS "${NETCDF_DIR}/include"
          HINTS ${NETCDF_INCLUDE_PATH})

find_library (NETCDF_LIBRARY NAMES netcdf
        PATHS "${NETCDF_DIR}/lib"
              "${NETCDF_INCLUDE_DIR}/../lib"
        HINTS "${NETCDF_PREFIX_DIR}/lib")

if(NETCDF_INCLUDE_DIR AND NETCDF_LIBRARY)
    set (NETCDF_C_INCLUDE_DIRS ${NETCDF_INCLUDE_DIR})
    set (NETCDF_C_LIBRARIES ${NETCDF_LIBRARY})

    find_file(NETCDF_MEM_H
             NAMES "netcdf_mem.h"
             HINTS ${NETCDF_INCLUDE_DIR})
    mark_as_advanced(NETCDF_MEM_H)
    if(NETCDF_MEM_H)
        set(NETCDF_HAS_MEM ON)
    endif()

    function(GET_NC_CONFIG _conf)
        if(_conf STREQUAL "c++")
            set(_key "CXX")
        else()
            string(TOUPPER ${_conf} _key)
        endif()
        execute_process(COMMAND ${NC_CONFIG} --has-${_conf}
                        RESULT_VARIABLE nc_res
                        OUTPUT_VARIABLE nc_has_${_conf}
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(nc_has_${_conf} STREQUAL "yes")
            set(NETCDF_HAS_${_key} ON  PARENT_SCOPE)
        else()
            set(NETCDF_HAS_${_key} OFF PARENT_SCOPE)
        endif()
    endfunction()

    if(NC_CONFIG AND NOT WIN32)
        GET_NC_CONFIG(dap)
        GET_NC_CONFIG(nc2)
        GET_NC_CONFIG(nc4)
        GET_NC_CONFIG(hdf4)
        GET_NC_CONFIG(hdf5)
    else()
        # Detect internal NC4_create symbol as a hint that NC4 support is enabled
        # Fragile...
        function(detect_NC4_create)
            include(CheckCSourceCompiles)
            set(CMAKE_REQUIRED_QUIET "yes")
            set(CMAKE_REQUIRED_LIBRARIES ${NETCDF_LIBRARY})
            check_c_source_compiles("int NC4_create ();int main () {return NC4_create ();}" HAVE_NC4_CREATE)
        endfunction()

        detect_NC4_create()
        if(HAVE_NC4_CREATE)
            set(NETCDF_HAS_NC4 ON)
        endif()
    endif()

    macro(NetCDF_DETECT_COMPONENT _comp)
        list (FIND NetCDF_FIND_COMPONENTS "${_comp}" _nextcomp)
        if (_nextcomp GREATER -1)
            set (NETCDF_${_comp} 1)
        endif ()
    endmacro()
    NetCDF_DETECT_COMPONENT("CXX")
    NetCDF_DETECT_COMPONENT("F77")
    NetCDF_DETECT_COMPONENT("F90")

    #start finding requested language components
    set (NetCDF_libs "")
    set (NetCDF_includes "${NETCDF_INCLUDE_DIR}")
    get_filename_component (NetCDF_lib_dirs "${NETCDF_LIBRARY}" PATH)
    set (NETCDF_HAS_INTERFACES "YES") # will be set to NO if we're missing any interfaces
    macro (NetCDF_check_interface lang header libs)
        if (NETCDF_${lang})
            #search starting from user modifiable cache var
            find_path (NETCDF_${lang}_INCLUDE_DIR NAMES ${header}
                    HINTS "${NETCDF_INCLUDE_DIR}"
                    HINTS "${NETCDF_${lang}_ROOT}/include"
                    ${USE_DEFAULT_PATHS})

            find_library (NETCDF_${lang}_LIBRARY NAMES ${libs}
                    HINTS "${NetCDF_lib_dirs}"
                    HINTS "${NETCDF_${lang}_ROOT}/lib"
                    ${USE_DEFAULT_PATHS})

            mark_as_advanced (NETCDF_${lang}_INCLUDE_DIR NETCDF_${lang}_LIBRARY)

            #export to internal varS that rest of project can use directly
            set (NETCDF_${lang}_LIBRARIES ${NETCDF_${lang}_LIBRARY})
            set (NETCDF_${lang}_INCLUDE_DIRS ${NETCDF_${lang}_INCLUDE_DIR})

            if (NETCDF_${lang}_INCLUDE_DIR AND NETCDF_${lang}_LIBRARY)
                list (APPEND NetCDF_libs ${NETCDF_${lang}_LIBRARY})
                list (APPEND NetCDF_includes ${NETCDF_${lang}_INCLUDE_DIR})
            else ()
                set (NETCDF_HAS_INTERFACES "NO")
                message (STATUS "Failed to find NetCDF interface for ${lang}")
            endif ()
        endif ()
    endmacro ()

    NetCDF_check_interface (CXX netcdfcpp.h netcdf_c++)
    NetCDF_check_interface (F77 netcdf.inc  netcdff)
    NetCDF_check_interface (F90 netcdf.mod  netcdff)

    #export accumulated results to internal vars that rest of project can depend on
    list (APPEND NetCDF_libs "${NETCDF_C_LIBRARIES}")
    set (NETCDF_LIBRARIES ${NetCDF_libs})
    set (NETCDF_INCLUDE_DIRS ${NetCDF_includes})
endif()
mark_as_advanced(NETCDF_LIBRARY NETCDF_INCLUDE_DIR)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (NetCDF
                                   FOUND_VAR NETCDF_FOUND
                                   REQUIRED_VARS NETCDF_LIBRARY NETCDF_INCLUDE_DIR
                                   VERSION_VAR NETCDF_VERSION
                                   )

if(NETCDF_FOUND)
    if(NOT TARGET NETCDF::netCDF)
        add_library(NETCDF::netCDF UNKNOWN IMPORTED)
        set_target_properties(NETCDF::netCDF PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NETCDF_INCLUDE_DIRS}")
        set_target_properties(NETCDF::netCDF PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C;CXX"
            IMPORTED_LOCATION "${NETCDF_LIBRARIES}")
    endif()
endif()
