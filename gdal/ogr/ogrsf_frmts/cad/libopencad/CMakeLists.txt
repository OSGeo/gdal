#*******************************************************************************
#  Project: libopencad
#  Purpose: OpenSource CAD formats support library
#  Author: Alexandr Borzykh, mush3d at gmail.com
#  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
#  Language: C++
#*******************************************************************************
#  The MIT License (MIT)
#
#  Copyright (c) 2016 Alexandr Borzykh
#  Copyright (c) 2016 NextGIS, <info@nextgis.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#*******************************************************************************

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
project(opencad)

include(util)

check_version(OCAD_MAJOR_VERSION OCAD_MINOR_VERSION OCAD_REV_VERSION)
set(VERSION ${OCAD_MAJOR_VERSION}.${OCAD_MINOR_VERSION}.${OCAD_REV_VERSION})
set(SOVERSION 1)

report_version(${PROJECT_NAME} ${VERSION})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(OBJ_LIB)

add_subdirectory(dwg)

set(HHEADERS
    opencad.h
    opencad_api.h
    cadfile.h
    cadfileio.h
    cadheader.h
    cadclasses.h
    cadtables.h
    cadgeometry.h
    cadlayer.h
    cadcolors.h
    caddictionary.h)

set(HHEADER_PRIV
    cadobjects.h
    cadfilestreamio.h
    )

set(CSOURCES
    opencad.cpp
    cadfile.cpp
    cadfileio.cpp
    cadfilestreamio.cpp
    cadheader.cpp
    cadclasses.cpp
    cadtables.cpp
    cadgeometry.cpp
    cadobjects.cpp
    cadlayer.cpp
    caddictionary.cpp)

set(LIB_NAME)
if(BUILD_SHARED_LIBS)
    set(LIB_TYPE SHARED)
    set(LIB_NAME ${PROJECT_NAME})
else()
    set(LIB_TYPE STATIC)
    set(LIB_NAME ${PROJECT_NAME}static)
endif()

add_library(${LIB_NAME} ${LIB_TYPE} ${CSOURCES} ${HHEADERS} ${HHEADER_PRIV} ${OBJ_LIB})

set(TARGET_LINK ${TARGET_LINK} ${LIB_NAME} PARENT_SCOPE)

if(BUILD_SHARED_LIBS)
    if(WIN32)
        set (LIB_VERSION ${OCAD_MAJOR_VERSION})
        if(MSVC)
            set_target_properties(${LIB_NAME} PROPERTIES IMPORT_SUFFIX "_i.lib")
        endif()
    else ()
      set (LIB_VERSION ${VERSION})
    endif()
    set_target_properties (${LIB_NAME} PROPERTIES VERSION ${LIB_VERSION})
    set_target_properties (${LIB_NAME} PROPERTIES SOVERSION ${SOVERSION})
    set_target_properties (${LIB_NAME} PROPERTIES DEFINE_SYMBOL OCAD_EXPORTS)
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    install(TARGETS ${LIB_NAME}
        EXPORT ${LIB_NAME}
        RUNTIME DESTINATION ${INSTALL_BIN_DIR} COMPONENT libraries
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
        LIBRARY DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
    )
endif()

if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(FILES ${HHEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT headers)
endif()
