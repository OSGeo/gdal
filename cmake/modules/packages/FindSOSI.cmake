# - Try to find the SOSI library
#
# Once done this will define
#
#  SOSI_FOUND - System has libgta
#  SOSI_INCLUDE_DIR - The libgta include directory
#  SOSI_LIBRARIES - The libraries needed to use libgta

find_path(SOSI_INCLUDE_DIR
          NAMES fyba.h
          PATH_SUFFIXES fyba)
find_library(SOSI_FYBA_LIBRARY NAMES fyba)
find_library(SOSI_FYGM_LIBRARY NAMES fygm)
find_library(SOSI_FYUT_LIBRARY NAMES fyut)
mark_as_advanced(SOSI_INCLUDE_DIR SOSI_FYBA_LIBRARY SOSI_FYGM_LIBRARY SOSI_FYUT_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SOSI
                                  REQUIRED_VARS SOSI_FYBA_LIBRARY SOSI_FYGM_LIBRARY SOSI_FYUT_LIBRARY SOSI_INCLUDE_DIR)
if(SOSI_FOUND)
    set(SOSI_LIBRARIES ${SOSI_FYBA_LIBRARY} ${SOSI_FYGM_LIBRARY} ${SOSI_FYUT_LIBRARY})
    set(SOSI_INCLUDE_DIRS ${SOSI_INCLUDE_DIR})
    if(NOT TARGET SOSI::SOSI)
        add_library(SOSI::SOSI UNKNOWN IMPORTED)
        set_target_properties(SOSI::SOSI PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${SOSI_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${SOSI_FYBA_LIBRARY})
        add_library(SOSI::FYGM UNKNOWN IMPORTED)
        set_target_properties(SOSI::FYGM PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${SOSI_FYGM_LIBRARY})
        add_library(SOSI::FYUT UNKNOWN IMPORTED)
        set_target_properties(SOSI::FYUT PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${SOSI_FYUT_LIBRARY})
    endif()
endif()
