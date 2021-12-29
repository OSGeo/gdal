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
  set(CSHARP_RID "osx-x64")
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

# setup local NuGet repository
if (DOTNET_FOUND)
  file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} _DN_REPO_PATH)
  dotnet_register_local_repository("local" ${_DN_REPO_PATH})
endif ()

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
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}" _CSHARP_DLL_TARGET)
  set(CSC_OPTIONS /unsafe /debug:full /target:library /out:${_CSHARP_DLL_TARGET})
  if (WIN32)
    list(APPEND CSC_OPTIONS /define:CLR4)
  endif ()

  # Setup dependencies
  set(_dlls)
  set(_projs)
  if (_CSHARP_DEPENDS)
    foreach (_dep IN LISTS _CSHARP_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      cmake_path(GET _dep FILENAME _filename)
      list(APPEND _dlls ${_filename}.dll)
      list(APPEND _projs ${_dep}.csproj)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll)
    endforeach ()
  endif (_CSHARP_DEPENDS)

  # run the c# build
  if (DOTNET_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll_template.csproj" "${_CSHARP_PROJ}")
    add_dotnet(
      ${_CSHARP_PROJ}
      ${CSHARP_CONFIG}
      NAME
      ${_CSHARP_TARGET}
      VERSION
      ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV}
      OUTPUT_PATH
      ${_CSHARP_PROJ_PATH}
      DEPENDS
      csharp_interface
      ${_dlls}
      SOURCES
      ${_projs})

  else (DOTNET_FOUND)
    message("BUILDING : " ${_CSHARP_TARGET} " : " ${CSHARP_COMPILER} ${CSC_OPTIONS} /out:${_CSHARP_TARGET})
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET}
      COMMAND
        ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DCSC_OPTIONS=${CSC_OPTIONS}"
        "-DCSHARP_COMPILER=${CSHARP_COMPILER}" "-DTARGET_SUBDIR=${_CSHARP_PROJ_PATH}"
        -DWORKING=${CMAKE_CURRENT_BINARY_DIR} -P "${CMAKE_CURRENT_SOURCE_DIR}/add_dll_xsc.cmake"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      VERBATIM
      DEPENDS csharp_interface ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk ${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs)
    add_custom_target(${_CSHARP_TARGET} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET})
    add_custom_command(
      TARGET ${_CSHARP_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo
              "copy ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET} ${CMAKE_CURRENT_BINARY_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}
              ${CMAKE_CURRENT_BINARY_DIR})
    if (_dlls)
      add_dependencies(${_CSHARP_TARGET} ${_dlls})
    endif ()
  endif (DOTNET_FOUND)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction ()

# ######################################################################################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props ${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props
               COPYONLY)

gdal_csharp_wrap(
  NAMESPACE
  OSGeo.GDAL
  WRAPPER
  gdalconst_wrap
  SWIG_INTERFACE
  ${PROJECT_SOURCE_DIR}/swig/include/gdalconst.i
  TARGET_SUBDIR
  const)

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

gdal_csharp_dll(TARGET gdalconst_csharp.dll TARGET_SUBDIR const)

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
  cmake_path(GET _GBCS_OUTPUT STEM _folder)
  set(_GBCS_PROJ_PATH ${CMAKE_CURRENT_BINARY_DIR}/${_folder})
  set(_GBCS_PROJ ${_GBCS_PROJ_PATH}/${_folder})
  file(TO_NATIVE_PATH "${_GBCS_PROJ}" _GBCS_PROJ_NATIVE)
  file(TO_NATIVE_PATH "${_GBCS_PROJ_PATH}" _GBCS_PROJ_PATH_NATIVE)
  file(MAKE_DIRECTORY ${_GBCS_PROJ_PATH})

  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}" _GBCS_OUTPUT_NATIVE)

  # Setup dependencies
  set(_dlls)
  set(_projs)
  if (_GBCS_DEPENDS)
    foreach (_dep IN LISTS _GBCS_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      cmake_path(GET _dep FILENAME _filename)
      list(APPEND _dlls ${_filename}.dll)
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
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/exe_template.csproj" "${_GBCS_PROJ}.csproj")
    add_dotnet(
      ${_GBCS_PROJ_NATIVE}.csproj
      ${CSHARP_CONFIG}
      NAME
      ${_GBCS_OUTPUT}
      VERSION
      ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}.${GDAL_VERSION_REV}
      OUTPUT_PATH
      ${_GBCS_PROJ_PATH_NATIVE}
      DEPENDS
      csharp_binding
      PACKAGE
      ${_GBCS_SYSTEM_DEPENDS}
      ${_GBCS_DEPENDS})

  else (DOTNET_FOUND)
    message(
      "BUILDING : "
      ${_GBCS_OUTPUT}
      " : "
      ${CSHARP_COMPILER}
      ${CSC_OPTIONS}
      /out:${_GBCS_OUTPUT_NATIVE}
      ${SOURCE_NATIVE})
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E echo "Building ${_GBCS_OUTPUT_NATIVE}"
      COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} /out:${_GBCS_OUTPUT_NATIVE} ${SOURCE_NATIVE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      VERBATIM
      DEPENDS csharp_binding ${_GBCS_SOURCE})
    add_custom_target(${_GBCS_OUTPUT} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT})
    add_custom_command(
      TARGET ${_GBCS_OUTPUT}
      POST_BUILD
      COMMAND -E echo "Built ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}")
    if (_dlls)
      add_dependencies(${_GBCS_OUTPUT} ${_dlls})
    endif ()
  endif (DOTNET_FOUND)

  set(_GCBS_IMPORTS)
  if (DOTNET_FOUND)
    list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.exe)
    list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.dll)
    install(
      DIRECTORY ${_GBCS_PROJ_PATH}
      COMPONENT csharp
      DESTINATION ${GDAL_CSHARP_INSTALL_DIR}
      FILES_MATCHING
      PATTERN "*.json")
  else ()
    list(APPEND _GCBS_IMPORTS ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT})
  endif ()

  install(
    FILES ${_GCBS_IMPORTS}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction ()

# Build the samples
gdal_build_csharp_sample(
  OUTPUT
  cs_ogrinfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS
  ogr_csharp
  osr_csharp)

gdal_build_csharp_sample(
  OUTPUT
  cs_createdata.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS
  ogr_csharp
  osr_csharp)

gdal_build_csharp_sample(OUTPUT OSRTransform.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs DEPENDS
                         osr_csharp)

gdal_build_csharp_sample(
  OUTPUT
  GDALRead.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALReadDirect.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALAdjustContrast.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetRasterIO.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(OUTPUT GDALWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs DEPENDS gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALDatasetWrite.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
                         DEPENDS gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALColorTable.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs DEPENDS
                         gdal_csharp)

gdal_build_csharp_sample(OUTPUT WKT2WKB.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs DEPENDS ogr_csharp)

gdal_build_csharp_sample(OUTPUT OGRGEOS.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs DEPENDS ogr_csharp)

gdal_build_csharp_sample(OUTPUT ReadXML.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs DEPENDS gdal_csharp)

gdal_build_csharp_sample(
  OUTPUT
  GDALInfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS
  gdal_csharp
  osr_csharp)

gdal_build_csharp_sample(OUTPUT GDALOverviews.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs DEPENDS
                         gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALCreateCopy.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs DEPENDS
                         gdal_csharp)

gdal_build_csharp_sample(OUTPUT GDALGetHistogram.exe SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
                         DEPENDS gdal_csharp)

add_custom_target(csharp_interface DEPENDS gdalconst_wrap osr_wrap ogr_wrap gdal_wrap)

# Custom target to build the C# sample apps
add_custom_target(
  csharp_samples ALL
  DEPENDS csharp_binding
          cs_ogrinfo.exe
          cs_createdata.exe
          OSRTransform.exe
          GDALRead.exe
          GDALReadDirect.exe
          GDALAdjustContrast.exe
          GDALDatasetRasterIO.exe
          GDALWrite.exe
          GDALDatasetWrite.exe
          GDALColorTable.exe
          WKT2WKB.exe
          OGRGEOS.exe
          ReadXML.exe
          GDALInfo.exe
          GDALOverviews.exe
          GDALCreateCopy.exe
          GDALGetHistogram.exe)

# Custom Target to make the c# bindings - will be run as part of ALL but can also be built seperately
add_custom_target(csharp_binding DEPENDS csharp_interface gdalconst_csharp.dll osr_csharp.dll ogr_csharp.dll
                                         gdal_csharp.dll)

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
