option(GDAL_CSHARP_ONLY "Compile C# bindings on existing GDAL installation" OFF)

if (GDAL_CSHARP_ONLY)
  find_package(GDAL REQUIRED)
else ()
  include(GdalStandardIncludes)
endif ()

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
    CACHE STRING "Config to be used to compile the C# artefacts > RELEASE|CONFIG")

set(_VERSION_STRING ${GDAL_VERSION_NO_DEV_SUFFIX})
if (GDAL_DEV_SUFFIX)
  set(_VERSION_STRING ${_VERSION_STRING}-${GDAL_DEV_SUFFIX})
endif ()

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
else ()
  set(CSHARP_DRAWING "System.Drawing")
endif ()

# setup local NuGet repository
if (DOTNET_FOUND)
  file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} _DN_REPO_PATH)
  dotnet_register_local_repository("local" ${_DN_REPO_PATH})
endif ()

# Cop[y build files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props ${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props
               COPYONLY)

# Crate some placmarker package for dependencies
if (DOTNET_FOUND)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/osgeo.gdal.core.csproj.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/osgeo.gdal.core.csproj")
  add_dotnet(
    ${CMAKE_CURRENT_BINARY_DIR}/osgeo.gdal.core.csproj
    ${CSHARP_CONFIG}
    NAME
    OSGeo.GDAL.Core
    VERSION
    ${_VERSION_STRING}
    OUTPUT_PATH
    ${CMAKE_CURRENT_BINARY_DIR})
else ()
  add_custom_target(OSGeo.GDAL.Core)
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
  if (GDAL_CSHARP_ONLY)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(${_CSHARP_WRAPPER} PRIVATE ${GDAL_LIBRARY})
  else ()
    gdal_standard_includes(${_CSHARP_WRAPPER})
    target_link_libraries(${_CSHARP_WRAPPER} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  endif ()

  install(
    TARGETS ${_CSHARP_WRAPPER}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})
endfunction (gdal_csharp_wrap)

# function for xxx_csharp.dll build
function (gdal_csharp_dll)

  # Setup
  set(_options)
  set(_oneValueArgs TARGET TARGET_SUBDIR WRAP_DLL PACKAGE_NAME)
  set(_multiValueArgs DEPENDS SYSTEM_DEPENDS)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  get_filename_component(_root ${_CSHARP_TARGET} NAME_WE)

  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj" _CSHARP_PROJ)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}" _CSHARP_PROJ_PATH)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}" _CSHARP_DLL_TARGET)
  set(CSC_OPTIONS /unsafe /debug:full /target:library /out:${_CSHARP_DLL_TARGET})
  if (WIN32)
    list(APPEND CSC_OPTIONS /define:CLR4)
  endif ()
  set(_PACKAGE_NAME ${_CSHARP_PACKAGE_NAME})

  # Setup dependencies
  set(_dlls)
  set(_deps csharp_interface)
  if (_CSHARP_DEPENDS)
    foreach (_dep IN LISTS _CSHARP_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      get_filename_component(_filename ${_dep} NAME)
      list(APPEND _dlls ${_filename}.dll)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll)
    endforeach ()
  endif (_CSHARP_DEPENDS)
  if (_CSHARP_SYSTEM_DEPENDS)
    foreach (_dep IN LISTS _CSHARP_SYSTEM_DEPENDS)
      if (_dep MATCHES "^OSGeo")
        list(APPEND _deps ${_dep})
      endif ()
    endforeach ()
  endif ()

  # run the c# build
  if (DOTNET_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll_template.csproj"
                   "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj.in")
    file(
      GENERATE
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj
      INPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj.in)
    add_custom_command(
      COMMAND ${CMAKE_COMMAND} -E "copy_if_different"
              "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj" "${_CSHARP_PROJ}"
      VERBATIM PRE_BUILD
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj"
      OUTPUT ${_CSHARP_PROJ})
    add_dotnet(
      ${_CSHARP_PROJ}
      ${CSHARP_CONFIG}
      NAME
      ${_PACKAGE_NAME}
      VERSION
      ${_VERSION_STRING}
      OUTPUT_PATH
      ${_CSHARP_PROJ_PATH}
      DEPENDS
      ${_deps}
      PACKAGE
      ${_CSHARP_SYSTEM_DEPENDS})

  else ()
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
    add_custom_target(${_PACKAGE_NAME} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET})
    add_custom_command(
      TARGET ${_PACKAGE_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo
              "copy ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET} ${CMAKE_CURRENT_BINARY_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}
              ${CMAKE_CURRENT_BINARY_DIR})
    if (_CSHARP_SYSTEM_DEPENDS)
      add_dependencies(${_PACKAGE_NAME} ${_CSHARP_SYSTEM_DEPENDS})
    endif ()
  endif ()

  set(_CSHARP_IMPORTS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET})
  if (DOTNET_FOUND)
    list(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${_PACKAGE_NAME}.${_VERSION_STRING}.nupkg)
  endif ()

  install(
    FILES ${_CSHARP_IMPORTS}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction ()

# ######################################################################################################################
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

gdal_csharp_dll(TARGET gdalconst_csharp.dll TARGET_SUBDIR const WRAP_DLL gdalconst_wrap PACKAGE_NAME OSGeo.GDAL.CONST)

gdal_csharp_dll(
  TARGET osr_csharp.dll
         TARGET_SUBDIR
         osr
         WRAP_DLL
         osr_wrap
         PACKAGE_NAME
         OSGeo.OSR
         SYSTEM_DEPENDS
         OSGeo.GDAL.Core)

gdal_csharp_dll(
  TARGET ogr_csharp.dll TARGET_SUBDIR ogr WRAP_DLL ogr_wrap
  DEPENDS osr_csharp
          gdalconst_csharp
          SYSTEM_DEPENDS
          OSGeo.OSR
          OSGeo.GDAL.CONST
          OSGeo.GDAL.Core
          PACKAGE_NAME
          OSGeo.OGR)

gdal_csharp_dll(
  TARGET gdal_csharp.dll TARGET_SUBDIR gdal
  DEPENDS osr_csharp
          ogr_csharp
          gdalconst_csharp
          WRAP_DLL
          gdal_wrap
          SYSTEM_DEPENDS
          OSGeo.OSR
          OSGeo.OGR
          OSGeo.GDAL.CONST
          OSGeo.GDAL.Core
          PACKAGE_NAME
          OSGeo.GDAL)

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
  get_filename_component(_folder ${_GBCS_OUTPUT} NAME_WE)
  set(_GBCS_PROJ_PATH ${CMAKE_CURRENT_BINARY_DIR}/${_folder})
  set(_GBCS_PROJ ${_GBCS_PROJ_PATH}/${_folder})
  file(TO_NATIVE_PATH "${_GBCS_PROJ}" _GBCS_PROJ_NATIVE)
  file(TO_NATIVE_PATH "${_GBCS_PROJ_PATH}" _GBCS_PROJ_PATH_NATIVE)
  file(MAKE_DIRECTORY ${_GBCS_PROJ_PATH})

  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT}" _GBCS_OUTPUT_NATIVE)

  # Setup dependencies
  set(_dlls)
  if (_GBCS_DEPENDS)
    foreach (_dep IN LISTS _GBCS_DEPENDS)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_dep}" _dep)
      get_filename_component(_filename ${_dep} NAME)
      list(APPEND _dlls ${_filename}.dll)
      list(APPEND CSC_OPTIONS /r:${_dep}.dll)
    endforeach ()
  endif (_GBCS_DEPENDS)
  if (_GBCS_SYSTEM_DEPENDS)
    foreach (_dep IN LISTS _GBCS_SYSTEM_DEPENDS)
      if (NOT _dep MATCHES "^OSGeo*.")
        list(APPEND CSC_OPTIONS /r:${_dep}.dll)
      endif ()
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
      OSGeo.GDAL.samples.${_folder}
      VERSION
      ${_VERSION_STRING}
      OUTPUT_PATH
      ${_GBCS_PROJ_PATH_NATIVE}
      DEPENDS
      csharp_binding
      PACKAGE
      ${_GBCS_SYSTEM_DEPENDS})

  else ()
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
    add_custom_target(${_GBCS_OUTPUT} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_GBCS_OUTPUT})
    if (_GBCS_SYSTEM_DEPENDS)
      foreach (_dep in LISTS _GBCS_SYSTEM_DEPENDS)
        if (_dep MATCHES "^OSGeo*")
          add_dependencies(${_GBCS_OUTPUT} ${_dep})
        endif ()
      endforeach ()
    endif ()
  endif ()

  set(_GCBS_IMPORTS)
  if (DOTNET_FOUND)
    if (WIN32)
      list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.exe)
    else ()
      list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ})
    endif ()
    list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.dll)
    list(APPEND ${_GBCS_PROJ}.nupkg)
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
  OgrInfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS
  ogr_csharp
  osr_csharp
  SYSTEM_DEPENDS
  OSGeo.OSR
  OSGeo.OGR)

gdal_build_csharp_sample(
  OUTPUT
  CreateData.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS
  ogr_csharp
  osr_csharp
  SYSTEM_DEPENDS
  OSGeo.OGR
  OSGeo.OSR)

gdal_build_csharp_sample(
  OUTPUT
  OSRTransform.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs
  DEPENDS
  osr_csharp
  SYSTEM_DEPENDS
  OSGeo.OSR)

gdal_build_csharp_sample(
  OUTPUT
  GDALRead.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALReadDirect.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALAdjustContrast.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetRasterIO.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL
  ${CSHARP_DRAWING})

gdal_build_csharp_sample(
  OUTPUT
  GDALWrite.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALDatasetWrite.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALColorTable.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  WKT2WKB.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs
  DEPENDS
  ogr_csharp
  SYSTEM_DEPENDS
  OSGeo.OGR)

gdal_build_csharp_sample(
  OUTPUT
  OGRGEOS.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs
  DEPENDS
  ogr_csharp
  SYSTEM_DEPENDS
  OSGeo.OGR)

gdal_build_csharp_sample(
  OUTPUT
  ReadXML.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALInfo.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS
  gdal_csharp
  osr_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL
  OSGeo.OSR)

gdal_build_csharp_sample(
  OUTPUT
  GDALOverviews.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALCreateCopy.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALGetHistogram.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

gdal_build_csharp_sample(
  OUTPUT
  GDALTest.exe
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALTest.cs
  DEPENDS
  gdal_csharp
  SYSTEM_DEPENDS
  OSGeo.GDAL)

# Custom Target to make the C# wrap Libraries and use SWIG to build the .cs files
add_custom_target(csharp_interface DEPENDS gdalconst_wrap osr_wrap ogr_wrap gdal_wrap)

# Custom Target to make the c# bindings - will be run as part of ALL but can also be built seperately
add_custom_target(csharp_binding ALL DEPENDS csharp_interface OSGeo.GDAL.CONST OSGeo.GDAL OSGeo.OGR OSGeo.OSR)

# Custom Target to build the C# bindings and the sample binaries
add_custom_target(
  csharp_samples
  DEPENDS csharp_binding
          OgrInfo.exe
          CreateData.exe
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
          GDALGetHistogram.exe
          GDALTest.exe)

# set up the tests
if ((CSHARP_INTERPRETER OR WIN32) AND NOT (GDAL_CSHARP_ONLY))
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
