#
# To be used by projects that make use of CMakeified hdf-4.2
#

#[=======================================================================[.rst:
FindHDF4
---------

Find the HDF4 includes and get all installed hdf4 library settings

Component supported:  XDR FORTRAN

The following vars are set if hdf4 is found.

.. variable:: HDF4_FOUND

  True if found, otherwise all other vars are undefined

.. variable:: HDF4_INCLUDE_DIR

  The include dir for main *.h files

.. variable:: HDF4_FORTRAN_INCLUDE_DIR

  The include dir for fortran modules and headers
  (Not yet implemented)

.. variable:: HDF4_VERSION_STRING

  full version (e.g. 4.2.0)

.. variable:: HDF4_VERSION_MAJOR

  major part of version (e.g. 4)

.. variable:: HDF4_VERSION_MINOR

  minor part (e.g. 2)

The following boolean vars will be defined

.. variable:: HDF4_ENABLE_PARALLEL

  1 if HDF4 parallel supported
  (Not yet implemented)

.. variable:: HDF4_BUILD_FORTRAN

  1 if HDF4 was compiled with fortran on

.. variable:: HDF4_BUILD_CPP_LIB

  1 if HDF4 was compiled with cpp on
  (Not yet implemented)

.. variable:: HDF4_BUILD_TOOLS

  1 if HDF4 was compiled with tools on
  (Not yet implemented)

Target names that are valid (depending on enabled options)
will be the following(Not yet implemented)

hdf              : HDF4 C library
hdf_f90cstub     : used by Fortran to C interface
hdf_fortran      : Fortran HDF4 library
mfhdf            : HDF4 multi-file C interface library
xdr              : RPC library
mfhdf_f90cstub   : used by Fortran to C interface to multi-file library
mfhdf_fortran    : Fortran multi-file library

#
#]=======================================================================]
#


include(SelectLibraryConfigurations)

set(HDF4_PATHS
    /usr/lib/hdf4
    /usr/share/hdf4
    /usr/local/hdf4
    /usr/local/hdf4/share
    )

find_path(HDF4_INCLUDE_DIR hdf.h
          PATHS ${HDF4_PATHS}
          PATH_SUFFIXES
          include
          Include
          hdf
          hdf4
          )

if(HDF4_INCLUDE_DIR AND EXISTS "${HDF4_INCLUDE_DIR}/hfile.h")
    file(STRINGS "${HDF4_INCLUDE_DIR}/hfile.h" hdf4_version_string
         REGEX "^#define[\t ]+LIBVER.*")
    string(REGEX MATCH "LIBVER_MAJOR[ \t]+([0-9]+)" "${hdf4_version_str}" HDF4_VERSION_MAJOR "${hdf4_version_string}")
    string(REGEX MATCH "LIBVER_MINOR[ \t]+([0-9]+)" "${hdf4_version_str}" HDF4_VERSION_MINOR "${hdf4_version_string}")
    string(REGEX MATCH "LIBVER_RELEASE[ \t]+([0-9]+)" "${hdf4_version_str}" HDF4_VERSION_RELEASE "${hdf4_version_string}")
    string(REGEX MATCH "LIBVER_SUBRELEASE[ \t]+([0-9A-Za-z]+)" "${hdf4_version_str}" HDF4_VERSION_SUBRELEASE "${hdf4_version_string}")
    unset(hdf4_version_string)
    if(HDF4_VERSION_SUBRELEASE STREQUAL "")
        set(HDF4_VERSION_STRING "${HDF4_VERSION_MAJOR}.${HDF4_VERSION_MINOR}.${HDF4_VERSION_RELEASE}_${HDF4_VERSION_SUBRELEASE}")
    else()
        set(HDF4_VERSION_STRING "${HDF4_VERSION_MAJOR}.${HDF4_VERSION_MINOR}.${HDF4_VERSION_RELEASE}")
    endif()
endif()

if(HDF4_INCLUDE_DIR)
    # Debian supplies the HDF4 library which does not conflict with NetCDF.
    # Test for Debian flavor first. Hint: install the libhdf4-alt-dev package.
    foreach(tgt IN ITEMS dfalt mfhdfalt df mfhdf hdf4)
        find_library(HDF4_${tgt}_LIBRARY_DEBUG
                     NAMES ${tgt}d
                     PATHS ${HDF4_PATHS}/lib)
        find_library(HDF4_${tgt}_LIBRARY_RELEASE
                     NAMES ${tgt}
                     PATHS ${HDF4_PATHS}/lib)
        select_library_configurations(HDF4_${tgt})
        mark_as_advanced(HDF4_${tgt}_LIBRARY HDF4_${tgt}_LIBRARY_RELEASE HDF4_${tgt}_LIBRARY_DEBUG )
    endforeach()
    if(HDF4_dfalt_LIBRARY AND HDF4_mfhdfalt_LIBRARY)
        set(HDF4_LIBRARY ${HDF4_dfalt_LIBRARY} ${HDF4_mfhdfalt_LIBRARY})
        if(NOT TARGET HDF4::HDF4)
            add_library(HDF4::HDF4 UNKNOWN IMPORTED)
            set_target_properties(HDF4::HDF4 PROPERTIES
                                  INTERFACE_INCLUDE_DIRECTORIES ${HDF4_INCLUDE_DIR}
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_dfalt_LIBRARY}")
            add_library(HDF4::MFHDF UNKNOWN IMPORTED)
            set_target_properties(HDF4::MFHDF PROPERTIES
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_mfhdfalt_LIBRARY}")
        endif()
    elseif(HDF4_df_LIBRARY AND HDF4_mfhdf_LIBRARY)
        set(HDF4_LIBRARY ${HDF4_df_LIBRARY} ${HDF4_mfhdf_LIBRARY})
        if(NOT TARGET HDF4::HDF4)
            add_library(HDF4::HDF4 UNKNOWN IMPORTED)
            set_target_properties(HDF4::HDF4 PROPERTIES
                                  INTERFACE_INCLUDE_DIRECTORIES ${HDF4_INCLUDE_DIR}
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_df_LIBRARY}")
            add_library(HDF4::MFHDF UNKNOWN IMPORTED)
            set_target_properties(HDF4::MFHDF PROPERTIES
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_mfhdf_LIBRARY}")
        endif()
    elseif(HDF4_hdf4_LIBRARY)
        set(HDF4_LIBRARY ${HDF4_hdf4_LIBRARY})
        if(NOT TARGET HDF4::HDF4)
            add_library(HDF4::HDF4 UNKNOWN IMPORTED)
            set_target_properties(HDF4::HDF4 PROPERTIES
                                  INTERFACE_INCLUDE_DIRECTORIES ${HDF4_INCLUDE_DIR}
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_hdf4_LIBRARY}")
        endif()
    endif()
    mark_as_advanced(HDF4_dfalt_LIBRARY HDF4_mfhdfalt_LIBRARY
                     HDF4_df_LIBRARY HDF4_mfhdf_LIBRARY
                     HDF4_hdf4_LIBRARY)

    list (FIND HDF4_FIND_COMPONENTS "XDR" _nextcomp)
    if (_nextcomp GREATER -1)
        find_library(HDF4_XDR_LIBRARY_DEBUG
                     NAMES xdrd
                     PATHS ${HDF4_PATHS}/lib)
        find_library(HDF4_XDR_LIBRARY_RELEASE
                     NAMES xdr
                     PATHS ${HDF4_PATHS}/lib)
        select_library_configurations(HDF4_XDR)
        mark_as_advanced(HDF4_XDR_LIBRARY_RELEASE HDF4_XDR_LIBRARY_DEBUG)
        if(NOT TARGET HDF4::XDR)
            add_library(HDF4::XDR UNKNOWN IMPORTED)
            set_target_properties(HDF4::XDR PROPERTIES
                                  IMPORTED_LINK_INTERFACE_LANGUAGES C
                                  IMPORTED_LOCATION "${HDF4_XDR_LIBRARY}")
        endif()
    endif()

    list (FIND HDF4_FIND_COMPONENTS "FORTRAN" _nextcomp)
    if (_nextcomp GREATER -1)
        find_path(HDF4_FORTRAN_INCLUDE_DIR hdf.f90
                  PATHS ${HDF4_PATHS}
                  PATH_SUFFIXES
                  include
                  Include
                  hdf
                  hdf4
                  )
        if(HDF4_FORTRAN_INCLUDE_DIR)
            find_library(HDF4_FORTRAN_LIBRARY
                         NAMES df_fortran
                         PATHS ${HDF4_PATHS}/lib)
            find_library(HDF4_FORTRAN_MF_LIBRARY
                         NAMES mfhdf_fortran
                         PATHS ${HDF4_PATHS}/lib)
            mark_as_advanced(HDF4_FORTRAN_LIBRARY HDF4_FORTRAN_MF_LIBRARY)
            if(NOT TARGET HDF4::FORTRAN)
                add_library(HDF4::FORTRAN UNKNOWN IMPORTED)
                set_target_properties(HDF4::FORTRAN PROPERTIES
                                      INTERFACE_INCLUDE_DIRECTORIES ${HDF4_INCLUDE_DIR}
                                      IMPORTED_LINK_INTERFACE_LANGUAGES C
                                      IMPORTED_LOCATION "${HDF4_XDR_LIBRARY}")
            endif()
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDF4
                                  FOUND_VAR HDF4_FOUND
                                  REQUIRED_VARS HDF4_LIBRARY HDF4_INCLUDE_DIR
                                  VERSION_VAR HDF4_VERSION
                                  HANDLE_COMPONENTS
                                  )

# set output variables
if(HDF4_FOUND)
    set(HDF4_LIBRARIES ${HDF4_LIBRARY})
    set(HDF4_INCLUDE_DIRS ${HDF4_INCLUDE_DIR})
endif()
