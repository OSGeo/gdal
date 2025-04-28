# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

# Increment the below number each time an ABI incompatible change is done,
# e.g removing a public function/method, changing its prototype (including
# adding a default value to a parameter of a C++ method), adding
# a new member or virtual function in a public C++ class, etc.
# This will typically happen for each GDAL feature release (change of X or Y in
# a X.Y.Z numbering scheme), but should not happen for a bugfix release (change of Z)
# Previous value: 37 for GDAL 3.11
set(GDAL_SOVERSION 37)

# Switches to control build targets(cached)
option(ENABLE_GNM "Build GNM (Geography Network Model) component" ON)
option(ENABLE_PAM "Set ON to enable Persistent Auxiliary Metadata (.aux.xml)" ON)
option(BUILD_APPS "Build command line utilities" ON)

# This option is to build drivers as plugins, for drivers that have external dependencies, that are not parf of GDAL
# core dependencies Examples are netCDF, HDF4, Oracle, PDF, etc. This global setting can be overridden at the driver
# level with GDAL_ENABLE_FRMT_{foo}_PLUGIN or OGR_ENABLE_{foo}_PLUGIN variables.
option(GDAL_ENABLE_PLUGINS "Set ON to build drivers that have non-core external dependencies as plugin" OFF)

# This option is to build drivers as plugins, for drivers that have no external dependencies or dependencies that are
# part of GDAL core dependencies Examples are BMP, FlatGeobuf, etc.
option(GDAL_ENABLE_PLUGINS_NO_DEPS "Set ON to build drivers that have no non-core external dependencies as plugin" OFF)
mark_as_advanced(GDAL_ENABLE_PLUGINS_NO_DEPS)

option(ENABLE_IPO "Enable Inter-Procedural Optimization if possible" OFF)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  option(GDAL_ENABLE_MACOSX_FRAMEWORK "Enable Framework on Mac OS X" OFF)
endif ()
option(GDAL_BUILD_OPTIONAL_DRIVERS "Whether to build GDAL optional drivers by default" ON)
option(OGR_BUILD_OPTIONAL_DRIVERS "Whether to build OGR optional drivers by default" ON)

# libgdal shared/satic library generation
option(BUILD_SHARED_LIBS "Set ON to build shared library" ON)

# produce position independent code, default is on when building a shared library
option(GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE "Set ON to produce -fPIC code" ${BUILD_SHARED_LIBS})

# Option to set preferred C# compiler
option(CSHARP_MONO "Whether to force the C# compiler to be Mono" OFF)

if (SSE2NEON_COMPILES)
  option(GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS "Set ON to use ARM Neon FPU optimizations" ON)
  if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
      message(STATUS "Using ARM Neon optimizations")
  endif()
endif()

# This line must be kept early in the CMake instructions. At time of writing,
# this file is populated only be scripts/install_bash_completions.cmake.in
install(CODE "file(REMOVE \"${PROJECT_BINARY_DIR}/install_manifest_extra.txt\")")

include(GdalCompilationFlags)

# ######################################################################################################################
# generate ${CMAKE_CURRENT_BINARY_DIR}/port/cpl_config.h

set(_CMAKE_C_FLAGS_backup ${CMAKE_C_FLAGS})
set(_CMAKE_CXX_FLAGS_backup ${CMAKE_CXX_FLAGS})

if (CMAKE_C_FLAGS)
  string(REPLACE "-Werror " " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
  string(REPLACE "/WX " " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
endif ()
if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
  string(REPLACE "/WX " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
endif ()
include(configure)

# generate ${CMAKE_CURRENT_BINARY_DIR}/gcore/gdal_version.h and set GDAL_VERSION variable
include(GdalVersion)

# find 3rd party libraries
include(CheckDependentLibraries)

# Generates now port/cpl_config.h (it depends on at least iconv detection in CheckDependentLibraries)
configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/cpl_config.h.in ${PROJECT_BINARY_DIR}/port/cpl_config.h @ONLY)

set(CMAKE_C_FLAGS ${_CMAKE_C_FLAGS_backup})
set(CMAKE_CXX_FLAGS ${_CMAKE_CXX_FLAGS_backup})

if (GDAL_HIDE_INTERNAL_SYMBOLS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif ()

# Check that all symbols we need are present in our dependencies This is in particular useful to check that drivers
# built as plugins can access all symbols they need.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  include(CheckLinkerFlag)
  check_linker_flag(C "-Wl,--no-undefined" HAS_NO_UNDEFINED)
  if (HAS_NO_UNDEFINED AND (NOT "${CMAKE_CXX_FLAGS}" MATCHES "-fsanitize") AND NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -Wl,--no-undefined")
  endif ()
endif ()

macro(set_alternate_linker linker)
  if( NOT "${USE_ALTERNATE_LINKER}" STREQUAL "${USE_ALTERNATE_LINKER_OLD_CACHED}" )
    unset(LINKER_EXECUTABLE CACHE)
  endif()
  find_program(LINKER_EXECUTABLE ld.${USE_ALTERNATE_LINKER} ${USE_ALTERNATE_LINKER})
  if(LINKER_EXECUTABLE)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      if( "${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL 12.0.0)
        add_link_options("--ld-path=${LINKER_EXECUTABLE}")
      else()
        add_link_options("-fuse-ld=${LINKER_EXECUTABLE}")
      endif()
    elseif( "${linker}" STREQUAL "mold" AND
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND
            "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 12.1.0)
      # GCC before 12.1.0: -fuse-ld does not accept mold as a valid argument,
      # so you need to use -B option instead.
      get_filename_component(_dir ${LINKER_EXECUTABLE} DIRECTORY)
      get_filename_component(_dir ${_dir} DIRECTORY)
      if( EXISTS "${_dir}/libexec/mold/ld" )
          add_link_options(-B "${_dir}/libexec/mold")
      else()
          message(FATAL_ERROR "Cannot find ${_dir}/libexec/mold/ld")
      endif()
    else()
      add_link_options("-fuse-ld=${USE_ALTERNATE_LINKER}")
    endif()
    message(STATUS "Using alternative linker: ${LINKER_EXECUTABLE}")
  else()
    message(FATAL_ERROR "Cannot find alternative linker ${USE_ALTERNATE_LINKER}")
  endif()
endmacro()

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  set(USE_ALTERNATE_LINKER "" CACHE STRING "Use alternate linker. Leave empty for system default; potential alternatives are 'gold', 'lld', 'bfd', 'mold'")
  if(NOT "${USE_ALTERNATE_LINKER}" STREQUAL "")
    set_alternate_linker(${USE_ALTERNATE_LINKER})
  endif()
  set(USE_ALTERNATE_LINKER_OLD_CACHED
      ${USE_ALTERNATE_LINKER}
      CACHE INTERNAL "Previous value of USE_ALTERNATE_LINKER")
endif()

if (ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT result)
    if (result)
      set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
    endif ()
endif ()

# ######################################################################################################################

set_property(GLOBAL PROPERTY gdal_private_link_libraries)
function (gdal_add_private_link_libraries)
  get_property(tmp GLOBAL PROPERTY gdal_private_link_libraries)
  foreach (arg ${ARGV})
    set(tmp ${tmp} ${arg})
  endforeach ()
  set_property(GLOBAL PROPERTY gdal_private_link_libraries ${tmp})
endfunction (gdal_add_private_link_libraries)

add_library(${GDAL_LIB_TARGET_NAME} gcore/gdal.h)

set(GDAL_LIB_OUTPUT_NAME
    "gdal"
    CACHE STRING "Name of the GDAL library")
# If a shared lib renaming has been set in ConfigUser.cmake
set_target_properties(${GDAL_LIB_TARGET_NAME} PROPERTIES OUTPUT_NAME ${GDAL_LIB_OUTPUT_NAME})

add_library(GDAL::GDAL ALIAS ${GDAL_LIB_TARGET_NAME})
add_dependencies(${GDAL_LIB_TARGET_NAME} generate_gdal_version_h)
if (M_LIB)
  gdal_add_private_link_libraries(-lm)
endif ()
# Set project and C++ Standard properties
set_target_properties(
  ${GDAL_LIB_TARGET_NAME}
  PROPERTIES PROJECT_LABEL ${PROJECT_NAME}
             VERSION ${GDAL_ABI_FULL_VERSION}
             SOVERSION "${GDAL_SOVERSION}"
             ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
             LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
             RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
             CXX_STANDARD 11
             CXX_STANDARD_REQUIRED YES)
set_property(TARGET ${GDAL_LIB_TARGET_NAME} PROPERTY PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gdalplugins")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gdalplugins")

if (MSVC)
  set(GDAL_DEBUG_POSTFIX
      "d"
      CACHE STRING "Postfix to add to the GDAL dll name for debug builds")
  set_target_properties(${GDAL_LIB_TARGET_NAME} PROPERTIES DEBUG_POSTFIX "${GDAL_DEBUG_POSTFIX}")
endif ()
if (MINGW AND BUILD_SHARED_LIBS)
    set_target_properties(${GDAL_LIB_TARGET_NAME} PROPERTIES SUFFIX "-${GDAL_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif ()

# Some of the types in our public headers are dependent on whether GDAL_DEBUG
# is defined or not
target_compile_definitions(${GDAL_LIB_TARGET_NAME} PUBLIC $<$<CONFIG:DEBUG>:GDAL_DEBUG>)

# Install properties
if (GDAL_ENABLE_MACOSX_FRAMEWORK)
  set(FRAMEWORK_VERSION ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR})
  set(FRAMEWORK_DESTINATION
      "Library/Frameworks"
      CACHE STRING "Framework destination sub-directory")
  set(FRAMEWORK_SUBDIR "${FRAMEWORK_DESTINATION}/gdal.framework/Versions/${FRAMEWORK_VERSION}")
  set(INSTALL_PLUGIN_DIR
      "${FRAMEWORK_SUBDIR}/PlugIns"
      CACHE PATH "Installation sub-directory for plugins")
  set(CMAKE_INSTALL_BINDIR
      "bin"
      CACHE STRING "Installation sub-directory for executables")
  set(CMAKE_INSTALL_LIBDIR
      "${FRAMEWORK_SUBDIR}"
      CACHE INTERNAL "Installation sub-directory for libraries" FORCE)
  # CMAKE_INSTALL_INCLUDEDIR should normally not be used
  set(CMAKE_INSTALL_INCLUDEDIR
      "${FRAMEWORK_SUBDIR}/Headers"
      CACHE INTERNAL "Installation sub-directory for headers" FORCE)
  set(CMAKE_INSTALL_DATADIR
      "${FRAMEWORK_SUBDIR}/Resources"
      CACHE INTERNAL "Installation sub-directory for resources" FORCE)
  set(GDAL_RESOURCE_PATH ${CMAKE_INSTALL_DATADIR})
  set_target_properties(
    ${GDAL_LIB_TARGET_NAME}
    PROPERTIES FRAMEWORK TRUE
               FRAMEWORK_VERSION ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}
               MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}
               MACOSX_FRAMEWORK_BUNDLE_VERSION "GDAL ${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}"
               MACOSX_FRAMEWORK_IDENTIFIER org.osgeo.libgdal
               XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
               INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${FRAMEWORK_DESTINATION}"
               BUILD_WITH_INSTALL_RPATH TRUE
               # MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/info.plist.in"
    )
else ()
  include(GNUInstallDirs)
  set(INSTALL_PLUGIN_DIR
      "${CMAKE_INSTALL_LIBDIR}/gdalplugins"
      CACHE PATH "Installation sub-directory for plugins")
  set(GDAL_RESOURCE_PATH ${CMAKE_INSTALL_DATADIR}/gdal)

  option(GDAL_SET_INSTALL_RELATIVE_RPATH "Whether the rpath of installed binaries should be written as a relative path to the library" OFF)
  if(GDAL_SET_INSTALL_RELATIVE_RPATH)
      if(APPLE)
        set(base @loader_path)
      else()
        set(base $ORIGIN)
      endif()

      file(RELATIVE_PATH relDir
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
      )
      if( NOT "${CMAKE_INSTALL_RPATH}" STREQUAL "" )
          message(WARNING "CMAKE_INSTALL_RPATH=${CMAKE_INSTALL_RPATH} will be ignored and replaced with ${base};${base}/${relDir} due to GDAL_SET_INSTALL_RELATIVE_RPATH being set")
      endif()
      set(CMAKE_INSTALL_RPATH ${base} ${base}/${relDir})
  endif()
endif ()

set(INSTALL_PLUGIN_FULL_DIR "${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGIN_DIR}")

function (is_sharp_embed_available res)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.21 AND
        ((CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0) OR
         (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 19.0)))
        # CMAKE_C_STANDARD=23 only supported since CMake 3.21
        set(TEST_SHARP_EMBED
          "static const unsigned char embedded[] = {\n#embed __FILE__\n};\nint main() { (void)embedded; return 0;}"
        )
        set(CMAKE_C_STANDARD_BACKUP "${CMAKE_C_STANDARD}")
        set(CMAKE_C_STANDARD "23")
        check_c_source_compiles("${TEST_SHARP_EMBED}" _TEST_SHARP_EMBED)
        set(CMAKE_C_STANDARD "${CMAKE_C_STANDARD_BACKUP}")
        if (_TEST_SHARP_EMBED)
            set(${res} ON PARENT_SCOPE)
        else()
            set(${res} OFF PARENT_SCOPE)
        endif()
    else()
        set(${res} OFF PARENT_SCOPE)
    endif()
endfunction()

is_sharp_embed_available(IS_SHARP_EMBED_AVAILABLE_RES)
if (NOT BUILD_SHARED_LIBS AND IS_SHARP_EMBED_AVAILABLE_RES)
    set(DEFAULT_EMBED_RESOURCE_FILES ON)
else()
    set(DEFAULT_EMBED_RESOURCE_FILES OFF)
endif()
option(EMBED_RESOURCE_FILES "Whether resource files should be embedded into the GDAL library (only available with a C23 compatible compiler)" ${DEFAULT_EMBED_RESOURCE_FILES})

if (EMBED_RESOURCE_FILES AND NOT IS_SHARP_EMBED_AVAILABLE_RES)
  message(FATAL_ERROR "C23 #embed not available with this compiler")
endif()

option(USE_ONLY_EMBEDDED_RESOURCE_FILES "Whether embedded resource files should be used (should nominally be used together with EMBED_RESOURCE_FILES=ON, otherwise this will result in non-functional builds)" OFF)

if (USE_ONLY_EMBEDDED_RESOURCE_FILES AND NOT EMBED_RESOURCE_FILES)
  message(WARNING "USE_ONLY_EMBEDDED_RESOURCE_FILES=ON set but EMBED_RESOURCE_FILES=OFF: some drivers will lack required resource files")
endif()

# Configure internal libraries
if (GDAL_USE_ZLIB_INTERNAL)
  option(RENAME_INTERNAL_ZLIB_SYMBOLS "Rename internal zlib symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_ZLIB_SYMBOLS)
  add_subdirectory(frmts/zlib)
endif ()
if (GDAL_USE_JSONC_INTERNAL)
  # Internal libjson symbols are renamed by default
  add_subdirectory(ogr/ogrsf_frmts/geojson/libjson)
endif ()

option(ENABLE_DEFLATE64 "Enable Deflate64 decompression" ON)
mark_as_advanced(ENABLE_DEFLATE64)
if(ENABLE_DEFLATE64)
    add_subdirectory(frmts/zlib/contrib/infback9)
endif()

# Internal zlib and jsonc must be declared before
add_subdirectory(port)

# JPEG options need to be defined before internal libtiff
if (GDAL_USE_JPEG_INTERNAL)
  option(RENAME_INTERNAL_JPEG_SYMBOLS "Rename internal libjpeg symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_JPEG_SYMBOLS)
  add_subdirectory(frmts/jpeg/libjpeg)
endif ()
if (NOT HAVE_JPEGTURBO_DUAL_MODE_8_12)
    option(GDAL_USE_JPEG12_INTERNAL "Set ON to use internal libjpeg12 support" ON)
else()
    option(GDAL_USE_JPEG12_INTERNAL "Set ON to use internal libjpeg12 support" OFF)
endif()
if (GDAL_USE_JPEG12_INTERNAL)
  add_subdirectory(frmts/jpeg/libjpeg12)
endif ()

# Lerc options need to be defined before internal libtiff
if (GDAL_USE_LERC_INTERNAL)
  # Internal liblerc uses a dedicated namespace
  add_subdirectory(third_party/LercLib)
endif ()

if (GDAL_USE_TIFF_INTERNAL)
  option(RENAME_INTERNAL_TIFF_SYMBOLS "Rename internal libtiff symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_TIFF_SYMBOLS)
  add_subdirectory(frmts/gtiff/libtiff)
endif ()
if (GDAL_USE_GEOTIFF_INTERNAL)
  option(RENAME_INTERNAL_GEOTIFF_SYMBOLS "Rename internal libgeotiff symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_GEOTIFF_SYMBOLS)
  add_subdirectory(frmts/gtiff/libgeotiff)
endif ()
if (GDAL_USE_PNG_INTERNAL)
  option(RENAME_INTERNAL_PNG_SYMBOLS "Rename internal libpng symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_PNG_SYMBOLS)
  add_subdirectory(frmts/png/libpng)
endif ()
if (GDAL_USE_SHAPELIB_INTERNAL)
  option(RENAME_INTERNAL_SHAPELIB_SYMBOLS "Rename internal Shapelib symbols" ON)
  mark_as_advanced(RENAME_INTERNAL_SHAPELIB_SYMBOLS)
endif ()

# Must be set before including ogr
option(OGR_ENABLE_DRIVER_TAB
       "Set ON to build MapInfo TAB and MIF/MID driver (required by Northwoord driver, and Shapefile attribute indexing)"
       ${OGR_BUILD_OPTIONAL_DRIVERS})
if(OGR_ENABLE_DRIVER_TAB AND
   NOT DEFINED OGR_ENABLE_DRIVER_TAB_PLUGIN AND
   GDAL_ENABLE_PLUGINS_NO_DEPS)
    option(OGR_ENABLE_DRIVER_TAB_PLUGIN "Set ON to build OGR MapInfo TAB and MIF/MID driver as plugin" ON)
endif()

# Precompiled header
if (USE_PRECOMPILED_HEADERS)
  include(GdalStandardIncludes)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gcore/empty_c.c" "")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gcore/empty.cpp" "")
  add_library(gdal_priv_header OBJECT "${CMAKE_CURRENT_BINARY_DIR}/gcore/empty_c.c" "${CMAKE_CURRENT_BINARY_DIR}/gcore/empty.cpp")
  gdal_standard_includes(gdal_priv_header)
  add_dependencies(gdal_priv_header generate_gdal_version_h)
  target_compile_options(gdal_priv_header PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX}> $<$<COMPILE_LANGUAGE:C>:${GDAL_C_WARNING_FLAGS}>)
  target_compile_definitions(gdal_priv_header PUBLIC $<$<CONFIG:DEBUG>:GDAL_DEBUG>)
  set_property(TARGET gdal_priv_header PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  target_precompile_headers(gdal_priv_header PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/gcore/gdal_priv.h>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_port.h>
  )
endif()

# Core components
add_subdirectory(alg)
add_subdirectory(ogr)
if (ENABLE_GNM)
  add_subdirectory(gnm)
endif ()

# Raster/Vector drivers (built-in and plugins)
set(GDAL_RASTER_FORMAT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/frmts")
set(GDAL_VECTOR_FORMAT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/ogr/ogrsf_frmts")

if(OGR_ENABLE_DRIVER_GPKG AND
   NOT DEFINED OGR_ENABLE_DRIVER_SQLITE AND
   DEFINED OGR_BUILD_OPTIONAL_DRIVERS AND
   NOT OGR_BUILD_OPTIONAL_DRIVERS)
   message(STATUS "Automatically enabling SQLite driver")
   set(OGR_ENABLE_DRIVER_SQLITE ON CACHE BOOL "Set ON to build OGR SQLite driver")
endif()

# We need to forward declare a few OGR drivers because raster formats need them
option(OGR_ENABLE_DRIVER_AVC "Set ON to build OGR AVC driver" ${OGR_BUILD_OPTIONAL_DRIVERS})
option(OGR_ENABLE_DRIVER_GML "Set ON to build OGR GML driver" ${OGR_BUILD_OPTIONAL_DRIVERS})
cmake_dependent_option(OGR_ENABLE_DRIVER_SQLITE "Set ON to build OGR SQLite driver" ${OGR_BUILD_OPTIONAL_DRIVERS}
                       "GDAL_USE_SQLITE3" OFF)
cmake_dependent_option(OGR_ENABLE_DRIVER_GPKG "Set ON to build OGR GPKG driver" ${OGR_BUILD_OPTIONAL_DRIVERS}
                       "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE" OFF)
cmake_dependent_option(OGR_ENABLE_DRIVER_MVT "Set ON to build OGR MVT driver" ${OGR_BUILD_OPTIONAL_DRIVERS}
                       "GDAL_USE_SQLITE3" OFF)

# Build frmts/iso8211 conditionally to drivers requiring it
if ((GDAL_BUILD_OPTIONAL_DRIVERS AND NOT DEFINED GDAL_ENABLE_DRIVER_ADRG AND NOT DEFINED GDAL_ENABLE_DRIVER_SDTS) OR
    GDAL_ENABLE_DRIVER_ADRG OR
    GDAL_ENABLE_DRIVER_SDTS OR
    (OGR_BUILD_OPTIONAL_DRIVERS AND NOT DEFINED OGR_ENABLE_DRIVER_S57 AND NOT DEFINED OGR_ENABLE_DRIVER_SDTS) OR
    OGR_ENABLE_DRIVER_S57 OR
    OGR_ENABLE_DRIVER_SDTS)
  add_subdirectory(frmts/iso8211)
endif()

add_subdirectory(frmts)
add_subdirectory(ogr/ogrsf_frmts)

# It needs to be after ogr/ogrsf_frmts so it can use OGR_ENABLE_SQLITE
add_subdirectory(gcore)

# Bindings
if (BUILD_SHARED_LIBS)
  add_subdirectory(swig)
endif ()

# Utilities
add_subdirectory(apps)

add_subdirectory(scripts)

# Add all library dependencies of target gdal
get_property(GDAL_PRIVATE_LINK_LIBRARIES GLOBAL PROPERTY gdal_private_link_libraries)
# GDAL_EXTRA_LINK_LIBRARIES may be set by the user if the various FindXXXX modules
# didn't capture all required dependencies (used for example by OSGeo4W)
target_link_libraries(${GDAL_LIB_TARGET_NAME} PRIVATE ${GDAL_PRIVATE_LINK_LIBRARIES} ${GDAL_EXTRA_LINK_LIBRARIES})

# Document/Manuals
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc")
  add_subdirectory(doc)
endif ()
add_subdirectory(man)

# So that GDAL can be used as a add_subdirectory() of another project
target_include_directories(
  ${GDAL_LIB_TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/apps>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/alg>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/gcore>
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/gcore/gdal_version_full>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/port>
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/port>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ogr>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ogr/ogrsf_frmts>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# MSVC specific resource preparation
if (MSVC)
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE gcore/Version.rc)
  source_group("Resource Files" FILES gcore/Version.rc)
endif ()

get_property(_plugins GLOBAL PROPERTY PLUGIN_MODULES)
add_custom_target(gdal_plugins DEPENDS ${_plugins})

# Install drivers.ini along with plugins
# We request the TARGET_FILE_DIR of one of the plugins, since the PLUGIN_OUTPUT_DIR will not contain the \Release suffix
# with MSVC generator
list(LENGTH _plugins PLUGIN_MODULES_LENGTH)
if (PLUGIN_MODULES_LENGTH GREATER_EQUAL 1)
  list(GET _plugins 0 FIRST_TARGET)
  set(PLUGIN_OUTPUT_DIR "$<TARGET_FILE_DIR:${FIRST_TARGET}>")
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/frmts/drivers.ini DRIVERS_INI_CONTENT)
  file(
    GENERATE
    OUTPUT ${PLUGIN_OUTPUT_DIR}/drivers.ini
    CONTENT ${DRIVERS_INI_CONTENT})
endif ()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/frmts/drivers.ini DESTINATION ${INSTALL_PLUGIN_DIR})

# ######################################################################################################################

# Note: this file is generated but not used.
configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/gdal_def.h.in ${CMAKE_CURRENT_BINARY_DIR}/gcore/gdal_def.h @ONLY)

# ######################################################################################################################
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY PUBLIC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/port/cpl_config.h)

set(GDAL_DATA_FILES
    LICENSE.TXT
    data/GDALLogoBW.svg
    data/GDALLogoColor.svg
    data/GDALLogoGS.svg
    data/gdalicon.png
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

# Copy all resource files from their source location to ${CMAKE_CURRENT_BINARY_DIR}/data
# Note that this is not only the small list of files set a few lines above,
# but also resource files attached to ${GDAL_LIB_TARGET_NAME} in other directories (drivers, etc.)
get_property(
  _data_files
  TARGET ${GDAL_LIB_TARGET_NAME}
  PROPERTY RESOURCE)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data")
foreach(_file IN LISTS _data_files)
    configure_file("${_file}" "${CMAKE_CURRENT_BINARY_DIR}/data" COPYONLY)
endforeach()

if (GDAL_ENABLE_MACOSX_FRAMEWORK)
  # We need to add data files and public headers as sources of the library os they get installed through the framework
  # installation mechanisms
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE "${GDAL_DATA_FILES}")
  get_property(
    _public_headers
    TARGET ${GDAL_LIB_TARGET_NAME}
    PROPERTY PUBLIC_HEADER)
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE "${_public_headers}")
endif ()

install(
  TARGETS ${GDAL_LIB_TARGET_NAME}
  EXPORT gdal-export
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RESOURCE DESTINATION ${GDAL_RESOURCE_PATH}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FRAMEWORK DESTINATION "${FRAMEWORK_DESTINATION}")

# Generate targets file for importing directly from GDAL build tree
export(TARGETS ${GDAL_LIB_TARGET_NAME}
        NAMESPACE GDAL::
        FILE "GDAL-targets.cmake")

if (NOT GDAL_ENABLE_MACOSX_FRAMEWORK)
  # Generate GdalConfig.cmake and GdalConfigVersion.cmake
  install(
    EXPORT gdal-export
    FILE GDAL-targets.cmake
    NAMESPACE GDAL::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gdal/
    EXPORT_LINK_INTERFACE_LIBRARIES)
  if (NOT BUILD_SHARED_LIBS)
    install(
      FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/GdalFindModulePath.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/DefineFindPackage2.cmake"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gdal/")
    include(GdalFindModulePath)
    foreach(dir IN LISTS GDAL_VENDORED_FIND_MODULES_CMAKE_VERSIONS ITEMS packages thirdparty)
      install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/${dir}"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gdal")
    endforeach()
  endif ()

  include(CMakePackageConfigHelpers)
  # SameMajorVersion as the C++ ABI stability is not relevant for new linking and there are only a few breaking API changes.
  write_basic_package_version_file(
    GDALConfigVersion.cmake
    VERSION ${GDAL_VERSION}
    COMPATIBILITY SameMajorVersion)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/GDALConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gdal/)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/template/GDALConfig.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/GDALConfig.cmake @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/GDALConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gdal/)

  # Generate gdal-config utility command and pkg-config module gdal.pc
  include(GdalGenerateConfig)
  gdal_generate_config(
    TARGET
    "${GDAL_LIB_TARGET_NAME}"
    GLOBAL_PROPERTY
    "gdal_private_link_libraries"
    GDAL_CONFIG
    "${PROJECT_BINARY_DIR}/apps/gdal-config"
    PKG_CONFIG
    "${CMAKE_CURRENT_BINARY_DIR}/gdal.pc")
  install(
    PROGRAMS ${PROJECT_BINARY_DIR}/apps/gdal-config
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT applications)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/gdal.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT libraries)
endif ()

configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/uninstall.cmake.in ${PROJECT_BINARY_DIR}/cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

################################################################
# Final reports and warnings
################################################################

if($ENV{GDAL_CMAKE_QUIET})
  set(GDAL_CMAKE_QUIET ON)
endif()

# Print summary
include(SystemSummary)
if(NOT GDAL_CMAKE_QUIET)
  system_summary(DESCRIPTION "GDAL is now configured on;")
endif()

# Do not warn about Shapelib being an optional package not found, as we don't recommend using it.
# Mono/DotNetFrameworkSdk is also an internal detail of CSharp that we don't want to report
get_property(_packages_not_found GLOBAL PROPERTY PACKAGES_NOT_FOUND)
set(_new_packages_not_found)
foreach (_package IN LISTS _packages_not_found)
  if (NOT ${_package} STREQUAL "Shapelib"
      AND NOT ${_package} STREQUAL "Podofo"
      AND NOT ${_package} STREQUAL "Mono"
      AND NOT ${_package} STREQUAL "DotNetFrameworkSdk")
    set(_new_packages_not_found ${_new_packages_not_found} "${_package}")
  endif ()
endforeach ()

include(FeatureSummary)
set_property(GLOBAL PROPERTY PACKAGES_NOT_FOUND ${_new_packages_not_found})
if(NOT GDAL_CMAKE_QUIET)
  feature_summary(DESCRIPTION "Enabled drivers and features and found dependency packages" WHAT ALL)
endif()
set_property(GLOBAL PROPERTY PACKAGES_NOT_FOUND ${_packages_not_found})

set(disabled_packages "")
get_property(_packages_found GLOBAL PROPERTY PACKAGES_FOUND)
foreach (_package IN LISTS _packages_found)
  string(TOUPPER ${_package} key)
  if (DEFINED GDAL_USE_${key} AND NOT GDAL_USE_${key})
    if (DEFINED GDAL_USE_${key}_INTERNAL)
      if (NOT GDAL_USE_${key}_INTERNAL)
        string(APPEND disabled_packages " * ${key} component has been detected, but is disabled with GDAL_USE_${key}=${GDAL_USE_${key}}, and the internal library is also disabled with GDAL_USE_${key}_INTERNAL=${GDAL_USE_${key}_INTERNAL}\n")
      endif()
    else ()
      string(APPEND disabled_packages " * ${key} component has been detected, but is disabled with GDAL_USE_${key}=${GDAL_USE_${key}}\n")
    endif()
  endif ()
endforeach ()
if (NOT GDAL_CMAKE_QUIET AND disabled_packages)
  message(STATUS "Disabled components:\n\n${disabled_packages}\n")
endif ()

set(internal_libs_used "")
foreach (_package IN LISTS _packages_found _new_packages_not_found)
  string(TOUPPER ${_package} key)
  if( GDAL_USE_${key}_INTERNAL )
      string(APPEND internal_libs_used " * ${key} internal library enabled\n")
  endif()
endforeach()
if (NOT GDAL_CMAKE_QUIET AND internal_libs_used)
  message(STATUS "Internal libraries enabled:\n\n${internal_libs_used}\n")
endif ()

if (NOT GDAL_CMAKE_QUIET AND
    DEFINED GDAL_USE_EXTERNAL_LIBS_OLD_CACHED)
  if (GDAL_USE_EXTERNAL_LIBS_OLD_CACHED AND NOT GDAL_USE_EXTERNAL_LIBS)
    message(
      WARNING
        "Setting GDAL_USE_EXTERNAL_LIBS=OFF after an initial invocation to ON may require to invoke CMake with \"-UGDAL_USE_*\""
      )
  endif ()
endif ()
set(GDAL_USE_EXTERNAL_LIBS_OLD_CACHED
    ${GDAL_USE_EXTERNAL_LIBS}
    CACHE INTERNAL "Previous value of GDAL_USE_EXTERNAL_LIBS")

# Emit a warning if users do not define the build type for non-multi config and that we can't find -O in CMAKE_CXX_FLAGS
# This is not super idiomatic to warn this way, but this will help users transitionning from autoconf where the default
# settings result in a -O2 build.
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT GDAL_CMAKE_QUIET
    AND NOT _isMultiConfig
    AND ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    AND (((CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
          AND (NOT ("${CMAKE_C_FLAGS}" MATCHES "-O") OR NOT ("${CMAKE_CXX_FLAGS}" MATCHES "-O"))) OR
         ((CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
          AND (("${CMAKE_C_FLAGS}" MATCHES "/Od") OR NOT ("${CMAKE_C_FLAGS}" MATCHES "/O")))))
  message(
    WARNING
      "CMAKE_BUILD_TYPE is not defined and CMAKE_C_FLAGS='${CMAKE_C_FLAGS}' and/or CMAKE_CXX_FLAGS='${CMAKE_CXX_FLAGS}' do not contain optimizing flags. Do not use in production! Using -DCMAKE_BUILD_TYPE=Release is suggested."
    )
endif ()

if (NOT GDAL_CMAKE_QUIET AND
    "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  message(WARNING "In-tree builds, that is running cmake from the top of the source tree are not recommended. You are advised instead to 'mkdir build; cd build; cmake ..'.")
endif()

if (NOT GDAL_CMAKE_QUIET
    AND UNIX # On Windows, Conda seems to be automatically used
    AND DEFINED ENV{CONDA_PREFIX}
    AND NOT "${CMAKE_PREFIX_PATH}" MATCHES "$ENV{CONDA_PREFIX}")
  message(WARNING "Environment variable CONDA_PREFIX=$ENV{CONDA_PREFIX} found, its value is not included in the content of the CMake variable CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}. You likely want to run \"${CMAKE_COMMAND} ${PROJECT_SOURCE_DIR} -DCMAKE_PREFIX_PATH=$ENV{CONDA_PREFIX}\"")
endif()


# vim: ts=4 sw=4 sts=4 et
