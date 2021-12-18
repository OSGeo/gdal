include(GdalStandardIncludes)
include(csharp.opt.cmake)

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REPLACE "/WX" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif ()

set(GDAL_CSHARP_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/csharp"
    CACHE PATH "Installation sub-directory for CSharp bindings")



# function for csharp build
function (gdal_csharp_dll)

  # Setup
  set(_options)
  set(_oneValueArgs WRAPPER SWIG_INTERFACE NAMESPACE TARGET TARGET_SUBDIR)
  set(_multiValueArgs DEPENDS)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(_CSHARP_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR})

  # Run the SWIG interface build
  add_custom_command(
    # create the sub folde
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_CSHARP_WORKING_DIRECTORY}

    #SWIG command
    COMMAND
      ${SWIG_EXECUTABLE} -namespace ${_CSHARP_NAMESPACE} -outdir ${_CSHARP_WORKING_DIRECTORY} -DSWIG2_CSHARP -dllimport
      ${_CSHARP_WRAPPER} -Wall -I${PROJECT_SOURCE_DIR}/swig/include -I${PROJECT_SOURCE_DIR}/swig/include/csharp
      -I${PROJECT_SOURCE_DIR}/gdal -c++ -csharp -o ${_CSHARP_WRAPPER}.cpp ${_CSHARP_SWIG_INTERFACE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${GDAL_SWIG_COMMON_INCLUDE} ${PROJECT_SOURCE_DIR}/swig/include/csharp/typemaps_csharp.i
            ${_CSHARP_SWIG_INTERFACE})

  # add the wrapper dll
  set_source_files_properties(${_CSHARP_WRAPPER} PROPERTIES GENERATED 1)
  add_library(${_CSHARP_WRAPPER} SHARED ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp)
  gdal_standard_includes(${_CSHARP_WRAPPER})
  target_link_libraries(${_CSHARP_WRAPPER} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

  #Setup dependencies
  set(_depends)
  if (_CSHARP_DEPENDS)
    set(_CSHARP_DEPENDS_ARGS "-DCSHARP_DEPENDS=${_CSHARP_DEPENDS}")
    string(REPLACE ";" "\\;" _CSHARP_DEPENDS_ARGS "${_CSHARP_DEPENDS_ARGS}")
    foreach (_dep IN LISTS _CSHARP_DEPENDS)
      list(APPEND _depends ${CMAKE_CURRENT_BINARY_DIR}/${_dep})
    endforeach ()
  endif ()

  # run the c# build from build_dll.cmake
  if( DOTNET_FOUND )
    set(_CSHARP_BUILD_COMMAND "build_dll.dotnet.cmake" )
  else()
    set(_CSHARP_BUILD_COMMAND "build_dll.xsc.cmake" )
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
    COMMAND
      ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DCSHARP_COMPILER=${CSHARP_COMPILER}" "-DCSHARP_TARGET=${_CSHARP_TARGET}" ${_CSHARP_DEPENDS_ARGS}
      "-DTARGET_SUBDIR=${_CSHARP_TARGET_SUBDIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/{_CSHARP_BUILD_COMMAND}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp ${_depends} ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk
            ${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs)

  install(
    TARGETS ${_CSHARP_WRAPPER}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

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

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "gdal;ogr;osr;const")

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

# Build the samples
gdal_build_csharp_sample(
  OUTPUT ogrinfo.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS ogr_csharp.dll osr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT createdata.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS ogr_csharp.dll osr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT OSRTransform.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs
  DEPENDS osr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALRead.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS gdal_csharp.dll
  SYSTEM_DEPENDS System.Drawing.dll)

gdal_build_csharp_sample(
  OUTPUT GDALReadDirect.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS gdal_csharp.dll
  SYSTEM_DEPENDS System.Drawing.dll)

gdal_build_csharp_sample(
  OUTPUT GDALAdjustContrast.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS gdal_csharp.dll
  SYSTEM_DEPENDS System.Drawing.dll)

gdal_build_csharp_sample(
  OUTPUT GDALDatasetRasterIO.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS gdal_csharp.dll
  SYSTEM_DEPENDS System.Drawing.dll)

gdal_build_csharp_sample(
  OUTPUT GDALWrite.exe 
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs
  DEPENDS gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetWrite.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
  DEPENDS
  gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALColorTable.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs
  DEPENDS gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT WKT2WKB.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs
  DEPENDS ogr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT OGRGEOS.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs
  DEPENDS ogr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT ReadXML.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs
  DEPENDS gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALInfo.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS gdal_csharp.dll osr_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALOverviews.exe 
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs
  DEPENDS gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALCreateCopy.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs
  DEPENDS gdal_csharp.dll)

gdal_build_csharp_sample(
  OUTPUT GDALGetHistogram.exe
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
  DEPENDS gdal_csharp.dll)

# Custom target to build the C# sample apps
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

# Custom Target to make the c# bindings - will be run as part of ALL but can also be built seperately
add_custom_target(
  csharp_binding ALL
  DEPENDS gdalconst_csharp.dll
          osr_csharp.dll
          ogr_csharp.dll
          gdal_csharp.dll
          gdalconst_wrap
          osr_wrap
          ogr_wrap
          gdal_wrap
          csharp_samples)

#set up the tests
if (CSHARP_INTERPRETER OR WIN32)
  include(GdalSetTestEnv)
  gdal_set_test_env(TEST_ENV)

  file(TO_NATIVE_PATH "/" _separator)

  if (WIN32)
    set(NEW_TEST_ENV)
    foreach (_env IN LISTS TEST_ENV)
      if (_env MATCHES "^PATH=.*$")
        string(REPLACE ";" "\\;" _env "${_env}")
        list(APPEND NEW_TEST_ENV "${_env}\\;$<SHELL_PATH:$<TARGET_FILE_DIR:gdal_wrap>>")
      else ()
        list(APPEND NEW_TEST_ENV "${_env}")
      endif ()
    endforeach ()
    set(TEST_ENV "${NEW_TEST_ENV}")
  endif ()


endif ()
