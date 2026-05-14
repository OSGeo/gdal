option(GDAL_CSHARP_ONLY "Compile C# bindings on existing GDAL installation" OFF)
# The following are used by downstream packagers.
option(CSHARP_BUILD_SAMPLES "Build C# sample applications" ${BUILD_TESTING})
option(CSHARP_RUN_TESTS "Run C# tests" ${CSHARP_BUILD_SAMPLES})
option(CSHARP_INSTALL_NUGET_PACKAGES "Install C# NuGet packages to the install location" ON)

if (GDAL_CSHARP_ONLY)
  find_package(GDAL REQUIRED)
else ()
  include(GdalStandardIncludes)
endif ()

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
  string(REPLACE "/WX " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
endif ()

set(GDAL_CSHARP_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/csharp"
    CACHE PATH "Installation sub-directory for CSharp bindings")

set(CSHARP_LIBRARY_VERSION
    "netstandard2.0"
    CACHE STRING ".NET framework to be used for libraries")
set(CSHARP_APPLICATION_VERSION
    "latest"
    CACHE STRING ".NET framework to be used for the sample Applications")

set(CSHARP_CONFIG
    "RELEASE"
    CACHE STRING "Config to be used to compile the C# artifacts > RELEASE|DEBUG")

message(STATUS "Available .NET runtimes: " "${DOTNET_RUNTIMES}")

# If "latest", resolve to highest available runtime
if("${CSHARP_APPLICATION_VERSION}" STREQUAL "latest")
    list(SORT DOTNET_RUNTIMES COMPARE NATURAL)
    list(REVERSE DOTNET_RUNTIMES)
    list(GET DOTNET_RUNTIMES 0 _LATEST_RUNTIME)

    # Convert to TFM format
    set(CSHARP_APPLICATION_VERSION "net${_LATEST_RUNTIME}")
endif()

message(STATUS "Requested Library .NET target framework: " ${CSHARP_LIBRARY_VERSION} )

string(REGEX MATCH "[0-9\.]+" _CSHARP_LIBRARY_TFM_NUMBER "${CSHARP_LIBRARY_VERSION}" )
string(REGEX MATCH "[0-9\.]+" _CSHARP_APPLICATION_TFM_NUMBER "${CSHARP_APPLICATION_VERSION}" )

set(_FOUND_COMPATIBLE_RUNTIME FALSE)

foreach(RT IN LISTS DOTNET_RUNTIMES)
    if(RT VERSION_GREATER_EQUAL _CSHARP_LIBRARY_TFM_NUMBER)
        set(_FOUND_COMPATIBLE_RUNTIME TRUE)
        break()
    endif()
endforeach()

if (_FOUND_COMPATIBLE_RUNTIME)
  message(STATUS ".NET Framework " ${CSHARP_LIBRARY_VERSION} " can be built" )
else ()
  message(STATUS ".NET Framework " ${CSHARP_LIBRARY_VERSION} " cannot be built with the available SDKs. C# Bindings will not be built" )
  RETURN ()
endif ()
if (CSHARP_BUILD_SAMPLES)
  message(STATUS "Requested Application .NET target framework: " ${CSHARP_APPLICATION_VERSION} )
  if ( NOT ( _CSHARP_LIBRARY_TFM_NUMBER STREQUAL _CSHARP_APPLICATION_TFM_NUMBER ) )
    
    set(_FOUND_COMPATIBLE_RUNTIME FALSE)

    foreach(RT IN LISTS DOTNET_RUNTIMES)
        if(RT VERSION_GREATER_EQUAL _CSHARP_APPLICATION_TFM_NUMBER)
            set(_FOUND_COMPATIBLE_RUNTIME TRUE)
            break()
        endif()
    endforeach()

    if ( _FOUND_COMPATIBLE_RUNTIME )
      message(STATUS ".NET Framework " ${CSHARP_APPLICATION_VERSION} " can be built" )
    else ()
      message(STATUS ".NET Framework " ${CSHARP_APPLICATION_VERSION} " cannot be built with the available SDKs. C# Bindings will not be built" )
      RETURN ()
    endif (_FOUND_COMPATIBLE_RUNTIME)  
  endif ()
else(CSHARP_BUILD_SAMPLES)
  message(STATUS "C# sample applications will not be built")
endif (CSHARP_BUILD_SAMPLES)

set(_VERSION_STRING ${GDAL_VERSION_NO_DEV_SUFFIX})
if (GDAL_DEV_SUFFIX)
  set(_VERSION_STRING ${_VERSION_STRING}-${GDAL_DEV_SUFFIX})
endif ()

# --- Detect architecture ---
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)

if(_proc MATCHES "^(x86_64|amd64)$")
    set(_arch "x64")
elseif(_proc MATCHES "^(aarch64|arm64)$")
    set(_arch "arm64")
elseif(_proc MATCHES "^(armv7|arm)$")
    set(_arch "arm")
elseif(_proc MATCHES "^(i[3-6]86|x86)$")
    set(_arch "x86")
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    set(_arch "x64")  # fallback
endif()

# --- Detect OS and build RID ---
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CSHARP_RID "osx-${_arch}")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CSHARP_RID "linux-${_arch}")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CSHARP_RID "win-${_arch}")
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

# needed because of differences in the packages

if(_CSHARP_APPLICATION_TFM_NUMBER LESS "6")
  set(CSHARP_DRAWING "System.Drawing.Common")
else ()
  set(CSHARP_DRAWING "System.Drawing.Primitives;SkiaSharp")
endif ()

# setup local NuGet repository
file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} _DN_REPO_PATH)
dotnet_register_local_repository("local" ${_DN_REPO_PATH})

# Copy build files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props ${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props
               COPYONLY)

# Create some placemarker packages for dependencies
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

list(APPEND GDAL_SWIG_COMMON_INTERFACE_FILES
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/gdal_csharp_extend.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/gdal_csharp.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/gnm_csharp.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/ogr_csharp_extend.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/ogr_csharp.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/osr_csharp.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/swig_csharp_extensions.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/typemaps_csharp.i
   ${PROJECT_SOURCE_DIR}/swig/include/csharp/csharp_strings.i)

# function for csharp wrapper build
function (gdal_csharp_wrap)
  # Setup
  set(_options)
  set(_oneValueArgs WRAPPER SWIG_INTERFACE NAMESPACE TARGET_SUBDIR)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  set(_CSHARP_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR})

  # Run the SWIG interface build
  add_custom_command(
    # create the sub folder
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_WRAPPER}.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_CSHARP_WORKING_DIRECTORY}
    # SWIG command
    COMMAND
      ${SWIG_EXECUTABLE} -namespace ${_CSHARP_NAMESPACE} -outdir ${_CSHARP_WORKING_DIRECTORY} -DSWIG2_CSHARP
      -DSWIG_CSHARP_NO_STRING_HELPER -dllimport ${_CSHARP_WRAPPER} -Wall -I${PROJECT_SOURCE_DIR}/swig/include 
      -I${PROJECT_SOURCE_DIR}/swig/include/csharp -I${PROJECT_SOURCE_DIR}/gdal -c++ -csharp -o ${_CSHARP_WRAPPER}.cpp
      ${_CSHARP_SWIG_INTERFACE}      
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES} ${PROJECT_SOURCE_DIR}/swig/include/csharp/typemaps_csharp.i
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

  if(WIN32)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj" _CSHARP_PROJ)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}" _CSHARP_PROJ_PATH)
      file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}" _CSHARP_DLL_TARGET)
  else()
      set(_CSHARP_PROJ "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj")
      set(_CSHARP_PROJ_PATH "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}")
      set(_CSHARP_DLL_TARGET "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET}")
  endif()

  set(_PACKAGE_NAME ${_CSHARP_PACKAGE_NAME})

  # Setup dependencies
  set(_deps csharp_interface)
  if (_CSHARP_SYSTEM_DEPENDS)
    foreach (_dep IN LISTS _CSHARP_SYSTEM_DEPENDS)
      if (_dep MATCHES "^OSGeo")
        list(APPEND _deps ${_dep})
      endif ()
    endforeach ()
  endif ()

  # run the c# build
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll_template.csproj"
                  "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj.in")
  file(
    GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}.csproj.in)
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E "copy_if_different"
            "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj" "${_CSHARP_PROJ}"
    VERBATIM
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_root}-$<CONFIG>.csproj"
    OUTPUT ${_CSHARP_PROJ})
  add_dotnet(
    ${_CSHARP_PROJ}
    ${CSHARP_CONFIG}
    NAME ${_PACKAGE_NAME}
    VERSION ${_VERSION_STRING}
    OUTPUT_PATH ${_CSHARP_PROJ_PATH}
    DEPENDS
      ${_deps}
    PACKAGE
      ${_CSHARP_SYSTEM_DEPENDS})

  set(_CSHARP_IMPORTS ${CMAKE_CURRENT_BINARY_DIR}/${_CSHARP_TARGET_SUBDIR}/${_CSHARP_TARGET})
  if (CSHARP_INSTALL_NUGET_PACKAGES)
    list(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${_PACKAGE_NAME}.${_VERSION_STRING}.nupkg)
  endif (CSHARP_INSTALL_NUGET_PACKAGES)

  install(
    FILES ${_CSHARP_IMPORTS}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction (gdal_csharp_dll)

# ######################################################################################################################
gdal_csharp_wrap(
  NAMESPACE OSGeo.GDAL
  WRAPPER gdalconst_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdalconst.i
  TARGET_SUBDIR const)

gdal_csharp_wrap(
  NAMESPACE OSGeo.OSR
  WRAPPER osr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/osr.i
  TARGET_SUBDIR osr)

gdal_csharp_wrap(
  NAMESPACE OSGeo.OGR
  WRAPPER ogr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/ogr.i
  TARGET_SUBDIR ogr)

gdal_csharp_wrap(
  NAMESPACE OSGeo.GDAL
  WRAPPER gdal_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdal.i
  TARGET_SUBDIR gdal)

gdal_csharp_dll(
  TARGET gdalconst_csharp.dll
  TARGET_SUBDIR const
  WRAP_DLL gdalconst_wrap
  PACKAGE_NAME OSGeo.GDAL.CONST)

gdal_csharp_dll(
  TARGET osr_csharp.dll
  TARGET_SUBDIR osr
  WRAP_DLL osr_wrap
  PACKAGE_NAME OSGeo.OSR
  SYSTEM_DEPENDS OSGeo.GDAL.Core)

gdal_csharp_dll(
  TARGET ogr_csharp.dll
  TARGET_SUBDIR ogr
  WRAP_DLL ogr_wrap
  DEPENDS osr_csharp 
          gdalconst_csharp
  SYSTEM_DEPENDS
          OSGeo.OSR
          OSGeo.GDAL.CONST
          OSGeo.GDAL.Core
  PACKAGE_NAME
          OSGeo.OGR)

gdal_csharp_dll(
  TARGET gdal_csharp.dll
  TARGET_SUBDIR gdal
  DEPENDS osr_csharp
          ogr_csharp
          gdalconst_csharp
  WRAP_DLL gdal_wrap
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

  # setup project file
  get_filename_component(_folder ${_GBCS_OUTPUT} NAME_WE)
  set(_GBCS_PROJ_PATH ${CMAKE_CURRENT_BINARY_DIR}/${_folder})
  set(_GBCS_PROJ ${_GBCS_PROJ_PATH}/${_folder})
  if(WIN32)
      file(TO_NATIVE_PATH "${_GBCS_PROJ}" _GBCS_PROJ_NATIVE)
      file(TO_NATIVE_PATH "${_GBCS_PROJ_PATH}" _GBCS_PROJ_PATH_NATIVE)
  else()
      set(_GBCS_PROJ_NATIVE "${_GBCS_PROJ}")
      set(_GBCS_PROJ_PATH_NATIVE "${_GBCS_PROJ_PATH}")
  endif()
  file(MAKE_DIRECTORY ${_GBCS_PROJ_PATH})

  set(OUTPUT_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/${_folder}/${_GBCS_OUTPUT}")
  if(WIN32)
      file(TO_NATIVE_PATH ${OUTPUT_FILENAME} _GBCS_OUTPUT_NATIVE)
  else()
      set(_GBCS_OUTPUT_NATIVE "${OUTPUT_FILENAME}")
  endif()

  if(WIN32)
      file(TO_NATIVE_PATH "${_GBCS_SOURCE}" SOURCE_NATIVE)
  else()
      set(SOURCE_NATIVE "${_GBCS_SOURCE}")
  endif()



  # build the sample exe
  if ( "SkiaSharp" IN_LIST _GBCS_SYSTEM_DEPENDS)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/exe_template_dcomp.csproj" "${_GBCS_PROJ}.csproj")
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/drawing_compat.cs
      ${_GBCS_PROJ_PATH}/drawing_compat.cs
      COPYONLY
    )
  else()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/exe_template.csproj" "${_GBCS_PROJ}.csproj")
  endif ()

  add_dotnet(
    ${_GBCS_PROJ_NATIVE}.csproj
    ${CSHARP_CONFIG}
    NAME OSGeo.GDAL.Samples.${_folder}
    VERSION ${_VERSION_STRING}
    OUTPUT_PATH ${_GBCS_PROJ_PATH_NATIVE}
    DEPENDS csharp_binding
    PACKAGE ${_GBCS_SYSTEM_DEPENDS})

  set(_GCBS_IMPORTS)
  if (WIN32)
    list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.exe)
  else ()
    list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ})
  endif ()
  list(APPEND _GCBS_IMPORTS ${_GBCS_PROJ}.dll)
  if (CSHARP_INSTALL_NUGET_PACKAGES)
    list(APPEND ${_GBCS_PROJ}.nupkg)
    install(
      DIRECTORY ${_GBCS_PROJ_PATH}
      COMPONENT csharp
      DESTINATION ${GDAL_CSHARP_INSTALL_DIR}
      FILES_MATCHING
      PATTERN "*.json")
  endif (CSHARP_INSTALL_NUGET_PACKAGES)
  install(
    FILES ${_GCBS_IMPORTS}
    COMPONENT csharp
    DESTINATION ${GDAL_CSHARP_INSTALL_DIR})

endfunction ()

# Custom Target to make the C# wrap Libraries and use SWIG to build the .cs files
add_custom_target(csharp_interface DEPENDS gdalconst_wrap osr_wrap ogr_wrap gdal_wrap)

# Custom Target to make the c# bindings - will be run as part of ALL but can also be built separately
add_custom_target(csharp_binding ALL DEPENDS csharp_interface OSGeo.GDAL.CONST OSGeo.GDAL OSGeo.OGR OSGeo.OSR)

if (CSHARP_BUILD_SAMPLES)

    # Build the samples
    gdal_build_csharp_sample(
      OUTPUT OgrInfo.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
      DEPENDS
        ogr_csharp
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.OSR
        OSGeo.OGR)

    gdal_build_csharp_sample(
      OUTPUT CreateData.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
      DEPENDS
        ogr_csharp
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR
        OSGeo.OSR)

    gdal_build_csharp_sample(
      OUTPUT OSRTransform.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs
      DEPENDS
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.OSR)

    gdal_build_csharp_sample(
      OUTPUT GDALRead.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        ${CSHARP_DRAWING})

    gdal_build_csharp_sample(
      OUTPUT GDALReadDirect.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        ${CSHARP_DRAWING})

    gdal_build_csharp_sample(
      OUTPUT GDALAdjustContrast.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        ${CSHARP_DRAWING})

    gdal_build_csharp_sample(
      OUTPUT GDALDatasetRasterIO.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        ${CSHARP_DRAWING})

    gdal_build_csharp_sample(
      OUTPUT GDALWrite.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALDatasetWrite.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALColorTable.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT WKT2WKB.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs
      DEPENDS
        ogr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR)

    gdal_build_csharp_sample(
      OUTPUT OGRGEOS.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs
      DEPENDS 
        ogr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR)

    gdal_build_csharp_sample(
      OUTPUT ReadXML.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALInfo.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
      DEPENDS
        gdal_csharp
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        OSGeo.OSR)

    gdal_build_csharp_sample(
      OUTPUT GDALOverviews.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALCreateCopy.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALGetHistogram.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALTest.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALTest.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GDALTestUtf8.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALTestUtf8.cs
      DEPENDS
        gdal_csharp
        ogr_csharp
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL
        OSGeo.OGR
        OSGeo.OSR)

    gdal_build_csharp_sample(
      OUTPUT OGRLayerAlg.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRLayerAlg.cs
      DEPENDS
        osr_csharp
        ogr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR
        OSGeo.OSR)

    gdal_build_csharp_sample(
      OUTPUT OGRFeatureEdit.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRFeatureEdit.cs
      DEPENDS
        osr_csharp
        ogr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR
        OSGeo.OSR)

    # Note the executable name change to avoid possible name clash
    gdal_build_csharp_sample(
      OUTPUT GDALCsharpWarp.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWarp.cs
      DEPENDS
        gdal_csharp
      SYSTEM_DEPENDS
        OSGeo.GDAL)

    gdal_build_csharp_sample(
      OUTPUT GetCRSInfo.exe
      SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apps/GetCRSInfo.cs
      DEPENDS
        osr_csharp
      SYSTEM_DEPENDS
        OSGeo.OGR)

    # Custom Target to build the C# bindings and the sample binaries
    add_custom_target(
      csharp_samples ALL
      DEPENDS csharp_binding
              OSGeo.GDAL.Samples.OgrInfo
              OSGeo.GDAL.Samples.CreateData
              OSGeo.GDAL.Samples.OSRTransform
              OSGeo.GDAL.Samples.GDALRead
              OSGeo.GDAL.Samples.GDALReadDirect
              OSGeo.GDAL.Samples.GDALAdjustContrast
              OSGeo.GDAL.Samples.GDALDatasetRasterIO
              OSGeo.GDAL.Samples.GDALWrite
              OSGeo.GDAL.Samples.GDALDatasetWrite
              OSGeo.GDAL.Samples.GDALColorTable
              OSGeo.GDAL.Samples.WKT2WKB
              OSGeo.GDAL.Samples.OGRGEOS
              OSGeo.GDAL.Samples.ReadXML
              OSGeo.GDAL.Samples.GDALInfo
              OSGeo.GDAL.Samples.GDALOverviews
              OSGeo.GDAL.Samples.GDALCreateCopy
              OSGeo.GDAL.Samples.GDALGetHistogram
              OSGeo.GDAL.Samples.GDALTestUtf8
              OSGeo.GDAL.Samples.GDALTest
              OSGeo.GDAL.Samples.OGRLayerAlg
              OSGeo.GDAL.Samples.GDALCsharpWarp
              OSGeo.GDAL.Samples.OGRFeatureEdit
              OSGeo.GDAL.Samples.GetCRSInfo.exe)

    # set up the tests
endif(CSHARP_BUILD_SAMPLES)
if (CSHARP_RUN_TESTS)

    include(GdalSetRuntimeEnv)
    gdal_set_runtime_env(TEST_ENV)

    add_test(
      NAME csharp_createdata
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/CreateData/CreateData Data pointlayer)
    set_property(TEST csharp_createdata PROPERTY ENVIRONMENT "${TEST_ENV}")
    add_test(
      NAME csharp_ogrinfo
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/OgrInfo/OgrInfo Data/pointlayer.shp)
    set_property(TEST csharp_ogrinfo PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_ogrinfo PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_osrtransform
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/OSRTransform/OSRTransform)
    set_property(TEST csharp_osrtransform PROPERTY ENVIRONMENT "${TEST_ENV}")
    add_test(
      NAME csharp_gdalwrite
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALWrite/GDALWrite Data/sample.tif)
    set_property(TEST csharp_gdalwrite PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdalwrite PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_gdaldatasetwrite
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALDatasetWrite/GDALDatasetWrite Data/sample1.tif)
    set_property(TEST csharp_gdaldatasetwrite PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdaldatasetwrite PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_gdalcreatecopy
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALCreateCopy/GDALCreateCopy Data/sample.tif Data/sample2.tif)
    set_property(TEST csharp_gdalcreatecopy PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdalcreatecopy PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_gdalreaddirect
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALReadDirect/GDALReadDirect Data/sample.tif Data/sample.png)
    set_property(TEST csharp_gdalreaddirect PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdalreaddirect PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_gdaloverviews
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALOverviews/GDALOverviews Data/sample.tif NEAREST 2 4)
    set_property(TEST csharp_gdaloverviews PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdaloverviews PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_gdalinfo
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALInfo/GDALInfo Data/sample.tif)
    set_property(TEST csharp_gdalinfo PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_gdalinfo PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_ogrlayeralg
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/OGRLayerAlg/OGRLayerAlg Intersection Data/pointlayer.shp pointlayer
              Data/pointlayer.shp pointlayer Data intersectionresult)
    set_property(TEST csharp_ogrlayeralg PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_ogrlayeralg PROPERTY DEPENDS csharp_createdata)
    add_test(
      NAME csharp_ogrlayeralgresult
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/OgrInfo/OgrInfo Data/intersectionresult.shp)
    set_property(TEST csharp_ogrlayeralgresult PROPERTY ENVIRONMENT "${TEST_ENV}")
    set_property(TEST csharp_ogrlayeralgresult PROPERTY DEPENDS csharp_ogrlayeralg)
    add_test( 
      NAME csharp_gdalwarp
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALCsharpWarp/GDALCsharpWarp Data/testwarp.tif "-of GTiff -t_srs EPSG:32645 -overwrite" "Data/byte.tif")
      set_property(TEST csharp_gdalwarp PROPERTY ENVIRONMENT "${TEST_ENV}")
      set_property(TEST csharp_gdalwarp PROPERTY DEPENDS csharp_createdata)
      file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../autotest/gcore/data/byte.tif DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Data/)
    add_test(
      NAME csharp_getcrsinfo
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GetCRSInfo/GetCRSInfo "EPSG" 20)
    set_property(TEST csharp_getcrsinfo PROPERTY ENVIRONMENT "${TEST_ENV}")
    add_test(
      NAME csharp_testutf8
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/GDALTestUtf8/GDALTestUtf8)
    set_property(TEST csharp_testutf8 PROPERTY ENVIRONMENT "${TEST_ENV}")
endif(CSHARP_RUN_TESTS)
