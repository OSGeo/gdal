include(GdalStandardIncludes)

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REPLACE "/WX" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif ()

set(GDAL_CSHARP_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/csharp"
    CACHE PATH "Installation sub-directory for CSharp bindings")

set(CSHARP_LIBRARY_VERSION
    "net5.0"
    CACHE STRING ".NET version to be used for libraries")
set(CSHARP_APPLICATION_VERSION
    "net5.0"
    CACHE STRING ".NET version to be used for the sample Applicatons")
set(CSHARP_CONFIG
    "RELEASE"
    CACHE STRING "Config to be used to compile the C# atrefacts > RELEASE|CONFIG")

# based on https://stackoverflow.com/questions/39258250/how-to-detect-if-64-bit-msvc-with-cmake TODO - this not going to
# cope with ARM architectures
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CSHARP_RID "osx-64")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CSHARP_RID "linux-x64")
elseif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
  set(CSHARP_RID "win-x64")
elseif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
  set(CSHARP_RID "win-32")
endif ()

# needed because of differnces in the packages
if (DOTNET_FOUND)
  set(CSHARP_DRAWING "System.Drawing.Common")
else (DOTNET_FOUND)
  set(CSHARP_DRAWING "System.Drawing")
endif (DOTNET_FOUND)

# function for csharp wrapper build
function (gdal_csharp_wrap)
  # Setup
  set(_options)
  set(_oneValueArgs WRAPPER SWIG_INTERFACE NAMESPACE TARGET_SUBDIR)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(_CSHARP_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR})

  # Run the SWIG interface build
  add_custom_command(
    # create the sub folde
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_CSHARP_WORKING_DIRECTORY}
    # SWIG command
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

  install(
    TARGETS ${_CSHARP_WRAPPER}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})
endfunction (gdal_csharp_wrap)

# function for xxx_csharp.dll build
function (gdal_csharp_dll)

  # Setup
  set(_options)
  set(_oneValueArgs TARGET TARGET_SUBDIR)
  set(_multiValueArgs DEPENDS)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})

  string(REPLACE ".dll" ".csproj" _CSHARP_PROJ ${_CSHARP_TARGET})
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_PROJ}" _CSHARP_PROJ)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}" _CSHARP_PROJ_PATH)
  set(CSC_OPTIONS /unsafe /debug:full /target:library /out:${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}.dll)
  if (WIN32)
    list(APPEND CSC_OPTIONS /define:CLR4)
  endif ()

  # Setup dependencies
  set(_dlls)
  set(_projs)
  if (_CSHARP_DEPENDS)
    foreach (_dep IN LISTS _CSHARP_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      list(APPEND _dlls ${_dep}.dll)
      list(APPEND _projs ${_dep}.csproj)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll})
    endforeach ()
  endif (_CSHARP_DEPENDS)

  # run the c# build
  if (DOTNET_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll_template.csproj" "${_CSHARP_PROJ}")
    add_dotnet(
      ${_CSHARP_PROJ}
      ${CSHARP_CONFIG}
      DEPENDS
      interface
      SOURCES
      ${_projs}
      BUILD_OPTIONS
      /o
      ${_CSHARP_PROJ_PATH})
  else (DOTNET_FOUND)
    file(GLOB _CSHARP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/*.cs")
    list(APPEND _CSHARP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs")
    set(_CSHARP_NATIVE_SOURCES)
    foreach (_src IN LISTS _CSHARP_SOURCES)
      file(TO_NATIVE_PATH "${_src}" _src)
      list(APPEND _CSHARP_NATIVE_SOURCES "${_src}")
    endforeach ()
    message("BUILDING : " ${_CSHARP_TARGET} " : " ${CSHARP_COMPILER} ${CSC_OPTIONS} ${_CSHARP_NATIVE_SOURCES})
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
      COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} ${_CSHARP_NATIVE_SOURCES}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      VERBATIM
      DEPENDS ${dlls} ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk ${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs)
    message("BUILT : " ${_CSHARP_TARGET})
  endif (DOTNET_FOUND)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction ()

# ######################################################################################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props ${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props
               COPYONLY)

gdal_csharp_wrap(
  NAMESPACE
  OSGeo.OSR
  WRAPPER
  osr_wrap
  SWIG_INTERFACE
  ${PROJECT_SOURCE_DIR}/swig/include/osr.i
  TARGET_SUBDIR
  osr)

gdal_csharp_wrap(
  NAMESPACE
  OSGeo.OGR
  WRAPPER
  ogr_wrap
  SWIG_INTERFACE
  ${PROJECT_SOURCE_DIR}/swig/include/ogr.i
  TARGET_SUBDIR
  ogr)

gdal_csharp_wrap(
  NAMESPACE
  OSGeo.GDAL
  WRAPPER
  gdal_wrap
  SWIG_INTERFACE
  ${PROJECT_SOURCE_DIR}/swig/include/gdal.i
  TARGET_SUBDIR
  gdal)

gdal_csharp_dll(TARGET osr_csharp.dll TARGET_SUBDIR osr)

gdal_csharp_dll(TARGET ogr_csharp.dll TARGET_SUBDIR ogr DEPENDS osr/osr_csharp)

gdal_csharp_dll(TARGET gdal_csharp.dll TARGET_SUBDIR gdal DEPENDS ogr/ogr_csharp osr/osr_csharp)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "gdal;ogr;osr;const")

# ######################################################################################################################
# sample commands

function (gdal_build_csharp_sample)

  # setup arguments
  set(_options)
  set(_oneValueArgs SOURCE OUTPUT)
  set(_multiValueArgs DEPENDS SYSTEM_DEPENDS)
  cmake_parse_arguments(_GBCS "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(CSC_OPTIONS)

  # setup project file
  string(REPLACE ".exe" ".csproj" _GBCS_PROJ ${_GBCS_OUTPUT})
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_PROJ}" _GBCS_PROJ)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" _GBCS_PROJ_PATH)

  # Setup dependencies
  set(_dlls)
  set(_projs)
  if (_GBCS_DEPENDS)
    foreach (_dep IN LISTS _GCBS_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      list(APPEND _dlls ${_dep}.dll)
      list(APPEND _projs ${_dep}.csproj)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll)
    endforeach ()
  endif (_GBCS_DEPENDS)
  if (_GBCS_SYSTEM_DEPENDS)
    foreach (_dep IN LISTS _GBCS_SYSTEM_DEPENDS)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll)
    endforeach ()
  endif (_GBCS_SYSTEM_DEPENDS)

  file(TO_NATIVE_PATH "${_GBCS_SOURCE}" SOURCE_NATIVE)

  # build the sample exe
  if (DOTNET_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/exe_template.csproj" "${_GBCS_PROJ}")
    add_dotnet(
      ${_GBCS_PROJ}
      ${CSHARP_CONFIG}
      DEPENDS
      csharp_binding
      PACKAGE
      ${_GBCS_SYSTEM_DEPENDS}
      SOURCES
      ${_projs}
      BUILD_OPTIONS
      /o
      ${_GBCS_PROJ_PATH})
  else (DOTNET_FOUND)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}
      COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} /out:${_GBCS_OUTPUT} ${SOURCE_NATIVE}
      DEPENDS ${_dlls$} ${_GBCS_SOURCE})
  endif (DOTNET_FOUND)
endfunction ()

# Build the samples
gdal_build_csharp_sample(
  OUTPUT
  ogrinfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS
  ogr/ogr_csharp
  osr/osr_csharp)

gdal_build_csharp_sample(
  OUTPUT
  createdata.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS
  ogr/ogr_csharp
  osr/osr_csharp)

gdal_build_csharp_sample(OUTPUT OSRTransform.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs DEPENDS
                         ${CMAKE_CURRENT_BINARY_DIR}/osr/osr_csharp)

gdal_build_csharp_sample(
  OUTPUT
  GDALRead.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS
  gdal/gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALReadDirect.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS
  gdal/gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALAdjustContrast.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS
  gdal/gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetRasterIO.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS
  gdal/gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(OUTPUT GDALWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs DEPENDS
                         gdal/gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALDatasetWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
                         DEPENDS gdal/gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALColorTable.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs DEPENDS
                         gdal/gdal_csharp)

gdal_build_csharp_sample(OUTPUT WKT2WKB.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs DEPENDS ogr/ogr_csharp)

gdal_build_csharp_sample(OUTPUT OGRGEOS.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs DEPENDS ogr/ogr_csharp)

gdal_build_csharp_sample(OUTPUT ReadXML.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs DEPENDS gdal/gdal_csharp)

gdal_build_csharp_sample(
  OUTPUT
  GDALInfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS
  gdal/gdal_csharp
  osr/osr_csharp)

gdal_build_csharp_sample(OUTPUT GDALOverviews.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs DEPENDS
                         gdal/gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALCreateCopy.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs DEPENDS
                         gdal/gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALGetHistogram.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
                         gdal/gdal_csharp)

add_custom_target(interface DEPENDS osr_wrap ogr_wrap gdal_wrap)

# Custom target to build the C# sample apps
add_custom_target(
  csharp_samples ALL
  DEPENDS interface
          csharp_binding
          ${CMAKE_CURRENT_BINARY_DIR}/ogrinfo.exe
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
add_custom_target(csharp_binding DEPENDS interface osr_csharp.dll ogr_csharp.dll gdal_csharp.dll)

# set up the tests
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
