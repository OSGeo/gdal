# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file gdal/LICENSE.TXT.

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
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gdal/swig/${_SWIG_BINDING}/extensions)
  set(SWIG_ARGS -Wall ${_SWIG_ARGS} -I${CMAKE_SOURCE_DIR}/gdal/swig/include -I${CMAKE_SOURCE_DIR}/gdal/swig/include/${_SWIG_BINDING})
  # for gdalconst.i
  gdal_swig_binding_target(
          TARGET gdalconst
          BINDING ${_SWIG_BINDING}
          ARGS ${SWIG_ARGS}
          OUTPUT ${SWIG_OUTPUT}
          DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES}
          ${_SWIG_DEPENDS}
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/${_SWIG_BINDING}/typemaps_${_SWIG_BINDING}.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/gdalconst.i
  )
  # for other interfaces
  foreach (tgt IN ITEMS gdal ogr osr gnm)
    gdal_swig_binding_target(
            TARGET ${tgt} CXX
            BINDING ${_SWIG_BINDING}
            ARGS ${SWIG_ARGS}
            OUTPUT ${SWIG_OUTPUT}
            DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES}
            ${_SWIG_DEPENDS}
            ${CMAKE_SOURCE_DIR}/gdal/swig/include/${_SWIG_BINDING}/typemaps_${_SWIG_BINDING}.i
            ${CMAKE_SOURCE_DIR}/gdal/swig/include/${tgt}.i
            ${CMAKE_SOURCE_DIR}/gdal/swig/include/${_SWIG_BINDING}/${tgt}_${_SWIG_BINDING}.i
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
    set(_OUTPUT ${CMAKE_BINARY_DIR}/gdal/swig/${_SWIG_BINDING}/extensions/${_SWIG_TARGET}_wrap.cpp)
  else ()
    set(_OUTPUT ${CMAKE_BINARY_DIR}/gdal/swig/${_SWIG_BINDING}/extensions/${_SWIG_TARGET}_wrap.c)
  endif ()
  add_custom_command(
          OUTPUT ${_OUTPUT} ${_SWIG_OUTPUT}
          COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${CMAKE_SOURCE_DIR}/gdal
          $<$<BOOL:${_SWIG_CXX}>:-c++> -${_SWIG_BINDING}
          -o ${_OUTPUT}
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/${_SWIG_TARGET}.i
          DEPENDS ${_SWIG_DEPENDS})
  set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
endfunction()
