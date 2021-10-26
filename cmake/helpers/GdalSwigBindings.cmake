# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
GdalSwigBindings
--------------------

Helper functions to build SWIG language bindings for GDAL/OGR.

#]=======================================================================]

find_package(SWIG REQUIRED)

function(gdal_swig_bindings)
  set(_options)
  set(_oneValueArgs BINDING)
  set(_multiValueArgs "ARGS;DEPENDS;OUTPUT")
  cmake_parse_arguments(_SWIG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/swig/${_SWIG_BINDING}/extensions)
  set(SWIG_ARGS -Wall ${_SWIG_ARGS} -I${PROJECT_SOURCE_DIR}/swig/include -I${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_BINDING})
  # for gdalconst.i
  gdal_swig_binding_target(
          TARGET gdalconst
          BINDING ${_SWIG_BINDING}
          ARGS ${SWIG_ARGS}
          OUTPUT ${_SWIG_OUTPUT}
          DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES}
          ${_SWIG_DEPENDS}
          ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_BINDING}/typemaps_${_SWIG_BINDING}.i
          ${PROJECT_SOURCE_DIR}/swig/include/gdalconst.i
  )
  # for other interfaces
  foreach (tgt IN ITEMS gdal ogr osr gnm)
    gdal_swig_binding_target(
            TARGET ${tgt} CXX
            BINDING ${_SWIG_BINDING}
            ARGS ${SWIG_ARGS}
            OUTPUT ${_SWIG_OUTPUT}
            DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES}
            ${_SWIG_DEPENDS}
            ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_BINDING}/typemaps_${_SWIG_BINDING}.i
            ${PROJECT_SOURCE_DIR}/swig/include/${tgt}.i
            ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_BINDING}/${tgt}_${_SWIG_BINDING}.i
    )
  endforeach ()
endfunction()

# internal function
function(gdal_swig_binding_target)
  set(_options CXX)
  set(_oneValueArgs "TARGET;BINDING")
  set(_multiValueArgs "ARGS;DEPENDS;OUTPUT")
  cmake_parse_arguments(_SWIG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if (_SWIG_CXX)
    set(_OUTPUT ${PROJECT_BINARY_DIR}/swig/${_SWIG_BINDING}/extensions/${_SWIG_TARGET}_wrap.cpp)
  else ()
    set(_OUTPUT ${PROJECT_BINARY_DIR}/swig/${_SWIG_BINDING}/extensions/${_SWIG_TARGET}_wrap.c)
  endif ()
  if ("${_SWIG_BINDING}" STREQUAL "python")
    set(BINDING_LANGUAGE_OUTPUT "${PROJECT_BINARY_DIR}/swig/${_SWIG_BINDING}/osgeo/${_SWIG_TARGET}.py")
  endif()
  add_custom_command(
          OUTPUT ${_OUTPUT} ${BINDING_LANGUAGE_OUTPUT}
          COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal
          $<$<BOOL:${_SWIG_CXX}>:-c++> -${_SWIG_BINDING}
          -o ${_OUTPUT}
          ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_TARGET}.i
          DEPENDS ${_SWIG_DEPENDS})
  set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
endfunction()
