# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

set(GDAL_SWIG_COMMON_INTERFACE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Band.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ColorTable.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cpl.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cpl_exceptions.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cplvirtualmem.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Dataset.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Driver.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gdal.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gdal_array.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gdal_typemaps.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gnm.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MajorObject.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ogr.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ogr_error_map.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Operations.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/osr.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RasterAttributeTable.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Relationship.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SubdatasetInfo.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Transform.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/XMLNode.i
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MultiDimensional.i)

# BUILD_PYTHON_BINDINGS is already defined in ../CMakeLists.txt

if (BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif ()

# Java bindings
if (SWIG_EXECUTABLE
    AND Java_JAVAC_EXECUTABLE
    AND JNI_FOUND
    AND ANT)
    option(BUILD_JAVA_BINDINGS "Build Java bindings" ON)
else()
    option(BUILD_JAVA_BINDINGS "Build Java bindings" OFF)
endif()

if (BUILD_JAVA_BINDINGS)
    if (NOT SWIG_FOUND)
        message(FATAL_ERROR "SWIG is a requirement to build the Java bindings")
    endif()
    if (NOT Java_JAVAC_EXECUTABLE)
        message(FATAL_ERROR "javac is a requirement to build the Java bindings")
    endif()
    if (NOT JNI_FOUND)
        message(FATAL_ERROR "JNI is a requirement to build the Java bindings")
    endif()
    if (NOT ANT)
        message(FATAL_ERROR "ant is a requirement to build the Java bindings")
    endif()
    add_subdirectory(java)
endif()

# CSharp bindings

if (SWIG_EXECUTABLE AND CSHARP_FOUND)
    option(BUILD_CSHARP_BINDINGS "Build CSharp bindings" ON)
else()
    option(BUILD_CSHARP_BINDINGS "Build CSharp bindings" OFF)
endif ()

option(BUILD_CSHARP_BINDINGS "Build CSharp bindings" ON)

if (BUILD_CSHARP_BINDINGS)
    if (NOT SWIG_FOUND)
        message(FATAL_ERROR "SWIG is a requirement to build the CSharp bindings")
    endif()
    if (NOT CSHARP_FOUND)
        message(FATAL_ERROR "A CSharp/DotNet compiler is a requirement to build the CSharp bindings")
    endif()
    add_subdirectory(csharp)
endif ()
