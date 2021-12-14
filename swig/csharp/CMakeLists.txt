include(GdalStandardIncludes)

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REPLACE "/WX" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif ()

# function for csharp build
function (gdal_csharp_dll)
  set(_options)
  set(_oneValueArgs WRAPPER SWIG_INTERFACE NAMESPACE TARGET TARGET_SUBDIR)
  set(_multiValueArgs DEPENDS)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(_CSHARP_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR})
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_CSHARP_WORKING_DIRECTORY}
    COMMAND
      ${SWIG_EXECUTABLE} -namespace ${_CSHARP_NAMESPACE} -outdir ${_CSHARP_WORKING_DIRECTORY} -DSWIG2_CSHARP -dllimport
      ${_CSHARP_WRAPPER} -Wall -I${PROJECT_SOURCE_DIR}/swig/include -I${PROJECT_SOURCE_DIR}/swig/include/csharp
      -I${PROJECT_SOURCE_DIR}/gdal -c++ -csharp -o ${_CSHARP_WRAPPER}.cpp ${_CSHARP_SWIG_INTERFACE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${GDAL_SWIG_COMMON_INCLUDE} ${PROJECT_SOURCE_DIR}/swig/include/csharp/typemaps_csharp.i
            ${_CSHARP_SWIG_INTERFACE})
  set_source_files_properties(${_CSHARP_WRAPPER} PROPERTIES GENERATED 1)

  set(_wrapper_name "${_CSHARP_WRAPPER}_csharp")
  add_library(${_wrapper_name} SHARED ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp)
  if (WIN32)
    set_target_properties(${_wrapper_name} PROPERTIES OUTPUT_NAME "${_CSHARP_WRAPPER}")
  endif ()
  gdal_standard_includes(${_wrapper_name})
  target_link_libraries(${_wrapper_name} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

  if (NOT WIN32)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}.config
         "<configuration>\n<dllmap dll=\"${_CSHARP_WRAPPER}\" target=\"${_wrapper_name}\"/>\n</configuration>\n")
  endif ()

  set(_depends)
  if (_CSHARP_DEPENDS)
    set(_CSHARP_DEPENDS_ARGS "-DCSHARP_DEPENDS=${_CSHARP_DEPENDS}")
    string(REPLACE ";" "\\;" _CSHARP_DEPENDS_ARGS "${_CSHARP_DEPENDS_ARGS}")
    foreach (_dep IN LISTS _CSHARP_DEPENDS)
      list(APPEND _depends ${CMAKE_CURRENT_BINARY_DIR}/${_dep})
    endforeach ()
  endif ()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
    COMMAND
      ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DCSHARP_COMPILER=${CSHARP_COMPILER}" "-DCSHARP_TARGET=${_CSHARP_TARGET}" ${_CSHARP_DEPENDS_ARGS}
      "-DTARGET_SUBDIR=${_CSHARP_TARGET_SUBDIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/build_dll.cmake"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp ${_depends} ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk
            ${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs)
endfunction ()

# ######################################################################################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk COPYONLY)

gdal_csharp_dll(
  TARGET gdalconst_csharp.dll
  NAMESPACE OSGeo.GDAL
  WRAPPER gdalconst_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdalconst.i TARGET_SUBDIR const)

gdal_csharp_dll(
  TARGET osr_csharp.dll
  NAMESPACE OSGeo.OSR
  WRAPPER osr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/osr.i TARGET_SUBDIR osr)

gdal_csharp_dll(
  TARGET ogr_csharp.dll
  NAMESPACE OSGeo.OGR
  WRAPPER ogr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/ogr.i TARGET_SUBDIR ogr
  DEPENDS osr_csharp.dll)

gdal_csharp_dll(
  TARGET gdal_csharp.dll
  NAMESPACE OSGeo.GDAL
  WRAPPER gdal_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdal.i TARGET_SUBDIR gdal
  DEPENDS ogr_csharp.dll osr_csharp.dll)

set_directory_properties(
  PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES
    "gdal;ogr;osr;const;gdal_csharp.dll.config;ogr_csharp.dll.config;osr_csharp.dll.config;gdalconst_csharp.dll.config")

# ######################################################################################################################
# sample commands

function (gdal_build_csharp_sample)
  set(_options)
  set(_oneValueArgs SOURCE OUTPUT)
  set(_multiValueArgs DEPENDS SYSTEM_DEPENDS)
  cmake_parse_arguments(_GBCS "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(CSC_OPTIONS)
  set(_depends)
  foreach (_dep IN LISTS _GBCS_DEPENDS)
    list(APPEND CSC_OPTIONS /r:${_dep})
    list(APPEND _depends "${CMAKE_CURRENT_BINARY_DIR}/${_dep}")
  endforeach ()
  foreach (_dep IN LISTS _GBCS_SYSTEM_DEPENDS)
    list(APPEND CSC_OPTIONS /r:${_dep})
  endforeach ()
  file(TO_NATIVE_PATH "${_GBCS_SOURCE}" SOURCE_NATIVE)

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}
    COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} /out:${_GBCS_OUTPUT} ${SOURCE_NATIVE}
    DEPENDS ${_depends} ${_GBCS_SOURCE})
endfunction ()

gdal_build_csharp_sample(
  OUTPUT
  ogrinfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS
  ogr_csharp.dll
  osr_csharp.dll)
gdal_build_csharp_sample(
  OUTPUT
  createdata.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS
  ogr_csharp.dll
  osr_csharp.dll)
gdal_build_csharp_sample(OUTPUT OSRTransform.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs DEPENDS
                         osr_csharp.dll)
gdal_build_csharp_sample(
  OUTPUT
  GDALRead.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS
  gdal_csharp.dll
  SYSTEM_DEPENDS
  System.Drawing.dll)
gdal_build_csharp_sample(
  OUTPUT
  GDALReadDirect.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS
  gdal_csharp.dll
  SYSTEM_DEPENDS
  System.Drawing.dll)
gdal_build_csharp_sample(
  OUTPUT
  GDALAdjustContrast.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS
  gdal_csharp.dll
  SYSTEM_DEPENDS
  System.Drawing.dll)
gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetRasterIO.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS
  gdal_csharp.dll
  SYSTEM_DEPENDS
  System.Drawing.dll)
gdal_build_csharp_sample(OUTPUT GDALWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs DEPENDS
                         gdal_csharp.dll)
gdal_build_csharp_sample(OUTPUT GDALDatasetWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
                         DEPENDS gdal_csharp.dll)
gdal_build_csharp_sample(OUTPUT GDALColorTable.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs DEPENDS
                         gdal_csharp.dll)
gdal_build_csharp_sample(OUTPUT WKT2WKB.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs DEPENDS ogr_csharp.dll)
gdal_build_csharp_sample(OUTPUT OGRGEOS.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs DEPENDS ogr_csharp.dll)
gdal_build_csharp_sample(OUTPUT ReadXML.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs DEPENDS gdal_csharp.dll)
gdal_build_csharp_sample(
  OUTPUT
  GDALInfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS
  gdal_csharp.dll
  osr_csharp.dll)
gdal_build_csharp_sample(OUTPUT GDALOverviews.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs DEPENDS
                         gdal_csharp.dll)
gdal_build_csharp_sample(OUTPUT GDALCreateCopy.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs DEPENDS
                         gdal_csharp.dll)
gdal_build_csharp_sample(OUTPUT GDALGetHistogram.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
                         DEPENDS gdal_csharp.dll)

add_custom_target(
  csharp_samples
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ogrinfo.exe
          ${CMAKE_CURRENT_BINARY_DIR}/createdata.exe
          ${CMAKE_CURRENT_BINARY_DIR}/OSRTransform.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALRead.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALReadDirect.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALAdjustContrast.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALDatasetRasterIO.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALWrite.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALDatasetWrite.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALColorTable.exe
          ${CMAKE_CURRENT_BINARY_DIR}/WKT2WKB.exe
          ${CMAKE_CURRENT_BINARY_DIR}/OGRGEOS.exe
          ${CMAKE_CURRENT_BINARY_DIR}/ReadXML.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALInfo.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALOverviews.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALCreateCopy.exe
          ${CMAKE_CURRENT_BINARY_DIR}/GDALGetHistogram.exe)
add_custom_target(csharp_binding ALL DEPENDS gdalconst_wrap_csharp osr_wrap_csharp ogr_wrap_csharp gdal_wrap_csharp
                                             csharp_samples)

if (CSHARP_INTERPRETER OR WIN32)
  include(GdalSetTestEnv)
  gdal_set_test_env(TEST_ENV)

  file(TO_NATIVE_PATH "/" _separator)

  if (WIN32)
    set(NEW_TEST_ENV)
    foreach (_env IN LISTS TEST_ENV)
      if (_env MATCHES "^PATH=.*$")
        string(REPLACE ";" "\\;" _env "${_env}")
        list(APPEND NEW_TEST_ENV "${_env}\\;$<SHELL_PATH:$<TARGET_FILE_DIR:gdal_wrap_csharp>>")
      else ()
        list(APPEND NEW_TEST_ENV "${_env}")
      endif ()
    endforeach ()
    set(TEST_ENV "${NEW_TEST_ENV}")
  endif ()

  add_test(NAME csharp_createdata COMMAND ${CSHARP_INTERPRETER} createdata.exe
                                          ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data pointlayer)
  add_test(NAME csharp_ogrinfo COMMAND ${CSHARP_INTERPRETER} ogrinfo.exe
                                       ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}pointlayer.shp)
  add_test(NAME csharp_GDALWrite COMMAND ${CSHARP_INTERPRETER} GDALWrite.exe
                                         ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample.tif)
  add_test(NAME csharp_GDALDatasetWrite COMMAND ${CSHARP_INTERPRETER} GDALDatasetWrite.exe
                                                ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample.tif)
  add_test(
    NAME csharp_GDALCreateCopy
    COMMAND
      ${CSHARP_INTERPRETER} GDALCreateCopy.exe ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample.tif
      ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample2.tif)
  add_test(NAME csharp_GDALOverviews
           COMMAND ${CSHARP_INTERPRETER} GDALOverviews.exe
                   ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample.tif NEAREST 2 4)
  add_test(NAME csharp_GDALInfo COMMAND ${CSHARP_INTERPRETER} GDALInfo.exe
                                        ${CMAKE_CURRENT_SOURCE_DIR}${_separator}Data${_separator}sample.tif)
  add_test(NAME csharp_OSRTransform COMMAND ${CSHARP_INTERPRETER} OSRTransform.exe)

  foreach (
    test_name IN
    ITEMS csharp_createdata
          csharp_ogrinfo
          csharp_GDALWrite
          csharp_GDALDatasetWrite
          csharp_GDALCreateCopy
          csharp_GDALOverviews
          csharp_GDALInfo
          csharp_OSRTransform)
    set_property(TEST ${test_name} PROPERTY ENVIRONMENT "${TEST_ENV}")
  endforeach ()

endif ()
