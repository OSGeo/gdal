# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGEOS
# -----------
#
# CMake module to search for GEOS library
#
# Copyright (C) 2017-2018, Hiroshi Miura
# Copyright (c) 2008, Mateusz Loskot <mateusz@loskot.net>
# (based on FindGDAL.cmake by Magnus Homann)
#
# If it's found it sets GEOS_FOUND to TRUE
# and following variables are set:
#    GEOS_INCLUDE_DIR
#    GEOS_LIBRARY
#

find_program(GEOS_CONFIG geos-config)
if(GEOS_CONFIG)
    execute_process(COMMAND "${GEOS_CONFIG}" --version OUTPUT_VARIABLE GEOS_VERSION)
    execute_process(COMMAND "${GEOS_CONFIG}" --prefix OUTPUT_VARIABLE GEOS_PREFIX)
endif()

find_path(GEOS_INCLUDE_DIR NAMES geos_c.h
          HINTS ${GEOS_PREFIX}/include)
find_library(GEOS_LIBRARY NAMES geos_c
             HINTS ${GEOS_PREFIX}/lib)
mark_as_advanced(GEOS_INCLUDE_DIR GEOS_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GEOS FOUND_VAR GEOS_FOUND
                                  REQUIRED_VARS GEOS_INCLUDE_DIR GEOS_LIBRARY
                                  GEOS_C_LIBRARY
                                  VERSION_VAR GEOS_VERSION)								  

if(GEOS_FOUND)
    set(GEOS_LIBRARIES ${GEOS_C_LIBRARY} ${GEOS_LIBRARY})
    set(GEOS_INCLUDE_DIRS ${GEOS_INCLUDE_DIR})
    set(GEOS_TARGET GEOS::GEOS)	

	if(NOT TARGET ${GEOS_TARGET})
			add_library(${GEOS_TARGET} UNKNOWN IMPORTED)
			set_target_properties(${GEOS_TARGET} PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${GEOS_INCLUDE_DIR}"
				IMPORTED_LINK_INTERFACE_LANGUAGES "C"
				
				# Set locations for Release, Debug, RelWithDebInfo, and MinSizeRel
				IMPORTED_LOCATION "${GEOS_LIBRARY}" 
				IMPORTED_LOCATION_RELEASE "${GEOS_LIBRARY}" 
				IMPORTED_LOCATION_DEBUG "${GEOS_LIBRARY}"
				IMPORTED_LOCATION_RELWITHDEBINFO "${GEOS_LIBRARY}"
				IMPORTED_LOCATION_MINSIZEREL "${GEOS_LIBRARY}"
			
				# Set the link libraries for all configurations
				IMPORTED_LINK_INTERFACE_LIBRARIES "${GEOS_LIBRARIES}"	# Link for Linux Build
				IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE "${GEOS_LIBRARIES}"	# Link for Release
				IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "${GEOS_LIBRARIES}"    # Link for Debug
				IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "${GEOS_LIBRARIES}"  
				IMPORTED_LINK_INTERFACE_LIBRARIES_MINSIZEREL "${GEOS_LIBRARIES}")  
    endif()
endif()
