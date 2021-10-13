# - Try to find the FME library
#
# Once done this will define
#
#  FME_FOUND - System has libgta
#  FME_INCLUDE_DIR - The libgta include directory
#  FME_LIBRARY - The libraries needed to use libgta

if(FME_HOME)
    FIND_PATH(FME_INCLUDE_DIR  fmeobjects/cpp/issesion.h
          HINTS ${FME_HOME})
endif()
mark_as_advanced(FME_INCLUDE_DIR FME_HOME)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FME REQUIRED_VARS FME_INCLUDE_DIR FME_HOME)
