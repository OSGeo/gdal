#
# To be used by projects that make use of CMakeified hdf-4.2
#

#
# Find the HDF4 includes and get all installed hdf4 library settings from
# HDF4-config.cmake file : Requires a CMake compatible hdf-4.2 or later
# for this feature to work. The following vars are set if hdf4 is found.
#
# HDF4_FOUND               - True if found, otherwise all other vars are undefined
# HDF4_INCLUDE_DIR         - The include dir for main *.h files
# HDF4_FORTRAN_INCLUDE_DIR - The include dir for fortran modules and headers
# HDF4_VERSION_STRING      - full version (e.g. 4.2.0)
# HDF4_VERSION_MAJOR       - major part of version (e.g. 4)
# HDF4_VERSION_MINOR       - minor part (e.g. 2)
#
# The following boolean vars will be defined
# HDF4_ENABLE_PARALLEL - 1 if HDF4 parallel supported
# HDF4_BUILD_FORTRAN   - 1 if HDF4 was compiled with fortran on
# HDF4_BUILD_CPP_LIB   - 1 if HDF4 was compiled with cpp on
# HDF4_BUILD_TOOLS     - 1 if HDF4 was compiled with tools on
#
# Target names that are valid (depending on enabled options)
# will be the following
#
# hdf              : HDF4 C library
# hdf_f90cstub     : used by Fortran to C interface
# hdf_fortran      : Fortran HDF4 library
# mfhdf            : HDF4 multi-file C interface library
# xdr              : RPC library
# mfhdf_f90cstub   : used by Fortran to C interface to multi-file library
# mfhdf_fortran    : Fortran multi-file library
#
# To aid in finding HDF4 as part of a subproject set
# HDF4_ROOT_DIR_HINT to the location where hdf4-config.cmake lies

include(FindPackageHandleStandardArgs)
include(SelectLibraryConfigurations)

# seed the initial lists of libraries to find with items we know we need
set( HDF4_C_LIBRARY_NAMES_INIT df dfalt)
set( HDF4_F90_LIBRARY_NAMES_INIT df_f90cstub ${HDF5_C_LIBRARY_NAMES_INIT} )
set( HDF4_FORTRAN_LIBRARY_NAMES_INIT df_fortran ${HDF4_F90_LIBRARY_NAMES_INIT} )
set( HDF4_MFHDF_LIBRARY_NAMES_INIT mfhdf mfhdfalt ${HDF4_C_LIBRARY_NAMES_INIT})
set( HDF4_XDR_LIBRARY_NAMES_INIT xdr ${HDF4_MFHDF_LIBRARY_NAMES_INIT})
set( HDF4_FORTRAN_MF_90_LIBRARY_NAMES_INIT mfhdf_f90cstub ${HDF4_FORTRAN_LIBRARY_NAMES_INIT} )
set( HDF4_FORTRAN_MF_LIBRARY_NAMES_INIT mfhdf_fortran ${HDF4_FORTRAN_MF_90_LIBRARY_NAMES_INIT} )
set( HDF4_C_LIB ${HDF4_XDR_LIBRARY_NAMES_INIT})

# The HINTS option should only be used for values computed from the system.
SET (_HDF4_HINTS
    $ENV{HOME}/.local
    $ENV{HDF4_ROOT}
    $ENV{HDF4_ROOT_DIR_HINT}
)
# Hard-coded guesses should still go in PATHS. This ensures that the user
# environment can always override hard guesses.
SET (_HDF4_PATHS
    $ENV{HOME}/.local
    $ENV{HDF4_ROOT}
    $ENV{HDF4_ROOT_DIR_HINT}
    /usr/lib/hdf4
    /usr/share/hdf4
    /usr/local/hdf4
    /usr/local/hdf4/share
)

FIND_PATH (HDF4_ROOT_DIR "hdf4-config.cmake"
    HINTS ${_HDF4_HINTS}
    PATHS ${_HDF4_PATHS}
    PATH_SUFFIXES
        cmake/hdf4
        lib/cmake/hdf4
        share/cmake/hdf4
)

FIND_PATH (HDF4_INCLUDE_DIRS "hdf.h"
    HINTS ${_HDF4_HINTS}
    PATHS ${_HDF4_PATHS}
    PATH_SUFFIXES
        include
        Include
        hdf
)

# For backwards compatibility we set HDF4_INCLUDE_DIR to the value of
# HDF4_INCLUDE_DIRS

if (HDF4_INCLUDE_DIRS)
  if(HDF4_ROOT_DIR)
    include (${HDF4_ROOT_DIR}/hdf4-config.cmake)
    set (HDF4_FOUND "YES")
  else()
    foreach( LIB ${HDF4_C_LIB} )
        if( UNIX AND HDF4_USE_STATIC_LIBRARIES )
            # According to bug 1643 on the CMake bug tracker, this is the
            # preferred method for searching for a static library.
            # See http://www.cmake.org/Bug/view.php?id=1643.  We search
            # first for the full static library name, but fall back to a
            # generic search on the name if the static search fails.
            set( THIS_LIBRARY_SEARCH_DEBUG lib${LIB}d.a ${LIB}d )
            set( THIS_LIBRARY_SEARCH_RELEASE lib${LIB}.a ${LIB} )
        else()
            set( THIS_LIBRARY_SEARCH_DEBUG ${LIB}d )
            set( THIS_LIBRARY_SEARCH_RELEASE ${LIB} )
        endif()
        find_library( HDF4_${LIB}_LIBRARY_DEBUG
            NAMES ${THIS_LIBRARY_SEARCH_DEBUG}
            HINTS ${HDF4_${LANGUAGE}_LIBRARY_DIRS}
            ENV HDF4_ROOT
            PATH_SUFFIXES lib Lib )
        find_library( HDF4_${LIB}_LIBRARY_RELEASE
            NAMES ${THIS_LIBRARY_SEARCH_RELEASE}
            HINTS ${HDF4_${LANGUAGE}_LIBRARY_DIRS}
            ENV HDF4_ROOT
            PATH_SUFFIXES lib Lib )
        select_library_configurations( HDF4_${LIB} )
        if(HDF4_${LIB}_LIBRARY)
            list(APPEND HDF4_LIBRARIES ${HDF4_${LIB}_LIBRARY})
        endif()
    endforeach()
    find_package_handle_standard_args(HDF4 DEFAULT_MSG HDF4_LIBRARIES HDF4_INCLUDE_DIRS)
    endif()
endif ()

IF(HDF4_FOUND)
  set(HDF4_LIBRARY ${HDF4_LIBRARIES})
  set(HDF4_INCLUDE_DIR ${HDF4_INCLUDE_DIRS})
ENDIF()

# Hide internal variables
mark_as_advanced(HDF4_INCLUDE_DIR HDF4_LIBRARY)
