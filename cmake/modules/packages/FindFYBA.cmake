# - Try to find the FYBA library
#
# Once done this will define
#
#  FYBA_FOUND - System has libgta
#  FYBA_INCLUDE_DIR - The libgta include directory
#  FYBA_LIBRARIES - The libraries needed to use libgta

find_path(FYBA_INCLUDE_DIR
          NAMES fyba.h
          PATH_SUFFIXES fyba)
find_library(FYBA_FYBA_LIBRARY NAMES fyba)
find_library(FYBA_FYGM_LIBRARY NAMES fygm)
find_library(FYBA_FYUT_LIBRARY NAMES fyut)
mark_as_advanced(FYBA_INCLUDE_DIR FYBA_FYBA_LIBRARY FYBA_FYGM_LIBRARY FYBA_FYUT_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FYBA
                                  REQUIRED_VARS FYBA_FYBA_LIBRARY FYBA_FYGM_LIBRARY FYBA_FYUT_LIBRARY FYBA_INCLUDE_DIR)
if(FYBA_FOUND)
    set(FYBA_LIBRARIES ${FYBA_FYBA_LIBRARY} ${FYBA_FYGM_LIBRARY} ${FYBA_FYUT_LIBRARY})
    set(FYBA_INCLUDE_DIRS ${FYBA_INCLUDE_DIR})
    if(NOT TARGET FYBA::FYBA)
        add_library(FYBA::FYBA UNKNOWN IMPORTED)
        set_target_properties(FYBA::FYBA PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${FYBA_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${FYBA_FYBA_LIBRARY})
        add_library(FYBA::FYGM UNKNOWN IMPORTED)
        set_target_properties(FYBA::FYGM PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${FYBA_FYGM_LIBRARY})
        add_library(FYBA::FYUT UNKNOWN IMPORTED)
        set_target_properties(FYBA::FYUT PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES C
                              IMPORTED_LOCATION ${FYBA_FYUT_LIBRARY})
    endif()
endif()
