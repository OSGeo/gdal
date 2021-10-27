include(GdalStandardIncludes)

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REPLACE "/WX" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif ()

function (GDAL_JAVA_WRAP)
  set(_options CXX)
  set(_oneValueArgs TARGET)
  set(_multiValueArgs ARGS)
  cmake_parse_arguments(_SWIG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if (_SWIG_CXX)
    set(SWIG_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.cpp)
    add_custom_command(
      OUTPUT ${SWIG_OUTPUT}
      COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal -c++ -java -o ${SWIG_OUTPUT}
              ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_TARGET}.i
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES} ${PROJECT_SOURCE_DIR}/swig/include/java/typemaps_java.i
              ${PROJECT_SOURCE_DIR}/swig/include/java/${_SWIG_TARGET}_java.i)
    set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
    add_library(${_SWIG_TARGET}jni MODULE ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.cpp)
  else ()
    set(SWIG_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.c)
    add_custom_command(
      OUTPUT ${SWIG_OUTPUT}
      COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal -java -o ${SWIG_OUTPUT}
              ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_TARGET}.i
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES} ${PROJECT_SOURCE_DIR}/swig/include/java/typemaps_java.i
              ${PROJECT_SOURCE_DIR}/swig/include/java/${_SWIG_TARGET}_java.i)
    set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
    add_library(${_SWIG_TARGET}jni MODULE ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.c)
  endif ()
  gdal_standard_includes(${_SWIG_TARGET}jni)
  target_include_directories(${_SWIG_TARGET}jni PRIVATE ${JNI_INCLUDE_DIRS} $<TARGET_PROPERTY:gnm,SOURCE_DIR>
                                                        ${CMAKE_CURRENT_BINARY_DIR})
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if (_isMultiConfig)
    set_target_properties(
      TARGET ${_SWIG_TARGET}jni PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/debug
                                           LIBRARY_OUTPUT_DIRECTORY_REL ${CMAKE_CURRENT_BINARY_DIR})
  else ()
    set_property(TARGET ${_SWIG_TARGET}jni PROPERTY LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif ()
  target_link_libraries(${_SWIG_TARGET}jni PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
endfunction ()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/gdal)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gdalconst)
gdal_java_wrap(
  TARGET gdalconst
  ARGS -Wall
       -I${PROJECT_SOURCE_DIR}/swig/include
       -I${PROJECT_SOURCE_DIR}/swig/include/java
       -outdir
       ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gdalconst/
       -package
       "org.gdal.gdalconst")
foreach (tgt IN ITEMS gdal ogr osr gnm)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${tgt})
  gdal_java_wrap(
    TARGET ${tgt}
    CXX
    ARGS -Wall
         -I${PROJECT_SOURCE_DIR}/swig/include
         -I${PROJECT_SOURCE_DIR}/swig/include/java
         -outdir
         ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${tgt}/
         -package
         "org.gdal.${tgt}")
endforeach ()

add_custom_target(java_binding ALL DEPENDS gdaljni ogrjni osrjni gnmjni gdalconstjni)
