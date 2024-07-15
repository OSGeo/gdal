# Distributed under the GDAL/OGR MIT style License.  See accompanying file LICENSE.TXT.

#[=======================================================================[.rst:
CheckDependentLibraries.cmake
-----------------------------

Detect GDAL dependencies and set variable HAVE_*

#]=======================================================================]

include(CheckFunctionExists)
include(CMakeDependentOption)
include(FeatureSummary)
include(DefineFindPackage2)
include(CheckSymbolExists)

option(
  GDAL_USE_EXTERNAL_LIBS
  "Whether detected external libraries should be used by default. This should be set before CMakeCache.txt is created."
  ON)

set(GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES ON OFF WHEN_NO_EXTERNAL)
set(
  GDAL_USE_INTERNAL_LIBS WHEN_NO_EXTERNAL
  CACHE STRING "Control how internal libraries should be used by default. This should be set before CMakeCache.txt is created.")
set_property(CACHE GDAL_USE_INTERNAL_LIBS PROPERTY STRINGS ${GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES})
if(NOT GDAL_USE_INTERNAL_LIBS IN_LIST GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES)
    message(FATAL_ERROR "GDAL_USE_INTERNAL_LIBS must be one of ${GDAL_USE_INTERNAL_LIBS_ALLOWED_VALUES}")
endif()

set(GDAL_IMPORT_DEPENDENCIES [[
include(CMakeFindDependencyMacro)
include("${CMAKE_CURRENT_LIST_DIR}/DefineFindPackage2.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GdalFindModulePath.cmake")
]])
if(TARGET Threads::Threads)
  string(APPEND GDAL_IMPORT_DEPENDENCIES "find_dependency(Threads)\n")
endif()

# Check that the configuration has a valid value for INTERFACE_INCLUDE_DIRECTORIES. This aimed at avoiding issues like
# https://github.com/OSGeo/gdal/issues/5324
function (gdal_check_target_is_valid target res_var)
  get_target_property(_interface_include_directories ${target} "INTERFACE_INCLUDE_DIRECTORIES")
  if(_interface_include_directories)
    foreach(_dir IN LISTS _interface_include_directories)
      if(NOT EXISTS "${_dir}")
        message(WARNING "Target ${target} references ${_dir} as a INTERFACE_INCLUDE_DIRECTORIES, but it does not exist. Ignoring that target.")
        set(${res_var} FALSE PARENT_SCOPE)
        return()
      endif()
    endforeach()
  elseif("${target}" STREQUAL "geotiff_library" AND DEFINED GeoTIFF_INCLUDE_DIRS)
    # geotiff-config.cmake of GeoTIFF 1.7.0 doesn't define a INTERFACE_INCLUDE_DIRECTORIES
    # property, but a GeoTIFF_INCLUDE_DIRS variable.
    set_target_properties(${target} PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${GeoTIFF_INCLUDE_DIRS}")
  else()
     message(WARNING "Target ${target} has no INTERFACE_INCLUDE_DIRECTORIES property. Ignoring that target.")
     set(${res_var} FALSE PARENT_SCOPE)
     return()
  endif()
  set(${res_var} TRUE PARENT_SCOPE)
endfunction()

# Package acceptance based on a candidate target list.
# If a matching target is found, sets ${name}_FOUND to TRUE,
# ${name}_INCLUDE_DIRS to "" and ${name}_LIBRARIES to the target name.
# If `REQUIRED` is used, ${name}_FOUND is set to FALSE if no target matches.
function(gdal_check_package_target name)
  if("REQUIRED" IN_LIST ARGN)
    list(REMOVE_ITEM ARGN "REQUIRED")
    set(${name}_FOUND FALSE PARENT_SCOPE)
  endif()
  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      gdal_check_target_is_valid(${target} _is_valid)
      if (_is_valid)
        set(${name}_TARGET "${target}" PARENT_SCOPE)
        set(${name}_FOUND TRUE PARENT_SCOPE)
        return()
      endif()
    endif()
  endforeach()
endfunction()

# Macro to declare a dependency on an external package.
# If not marked with the ALWAYS_ON_WHEN_FOUND option, dependencies can be
# marked for user control with either the CAN_DISABLE or DISABLED_BY_DEFAULT
# option. User control is done via a cache variable GDAL_USE_{name in upper case}
# with the default value ON for CAN_DISABLE or OFF for DISABLED_BY_DEFAULT.
# The RECOMMENDED option is used for the feature summary.
# The VERSION, CONFIG, MODULE, COMPONENTS and NAMES parameters are passed to find_package().
# Using NAMES with find_package() implies config mode. However, gdal_check_package()
# attempts another find_package() without NAMES if the config mode attempt was not
# successful, allowing a fallback to Find modules.
# The TARGETS parameter can define a list of candidate targets. If given, a
# package will only be accepted if it defines one of the given targets. The matching
# target name will be saved in ${name}_TARGET.
# The NAMES and TARGETS map to GDAL_CHECK_PACKAGE_${name}_NAMES and
# GDAL_CHECK_PACKAGE_${name}_TARGETS cache variables which can be used to
# overwrite the default config and targets names.
# The required find_dependency() commands for exported config are appended to
# the GDAL_IMPORT_DEPENDENCIES string (when BUILD_SHARED_LIBS=OFF).
macro (gdal_check_package name purpose)
  set(_options CONFIG MODULE CAN_DISABLE RECOMMENDED DISABLED_BY_DEFAULT ALWAYS_ON_WHEN_FOUND)
  set(_oneValueArgs VERSION NAMES)
  set(_multiValueArgs COMPONENTS TARGETS PATHS)
  cmake_parse_arguments(_GCP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  string(TOUPPER ${name} key)
  set(_find_dependency "")
  set(_find_dependency_args "")
  if(FIND_PACKAGE2_${name}_ENABLED)
    find_package2(${name} QUIET OUT_DEPENDENCY _find_dependency)
  else()
    set(_find_package_args)
    # For some reason passing the HDF5 version requirement cause a linking error of the libkea driver on Conda Windows builds...
    if (_GCP_VERSION AND NOT ("${name}" STREQUAL "TileDB") AND NOT ("${name}" STREQUAL "HDF5"))
      list(APPEND _find_package_args ${_GCP_VERSION})
    endif ()
    if (_GCP_CONFIG)
      list(APPEND _find_package_args CONFIG)
    endif ()
    if (_GCP_MODULE)
      list(APPEND _find_package_args MODULE)
    endif ()
    if (_GCP_COMPONENTS)
      list(APPEND _find_package_args COMPONENTS ${_GCP_COMPONENTS})
    endif ()
    if (_GCP_PATHS)
      list(APPEND _find_package_args PATHS ${_GCP_PATHS})
    endif ()
    if (_GCP_NAMES)
      set(GDAL_CHECK_PACKAGE_${name}_NAMES "${_GCP_NAMES}" CACHE STRING "Config file name for ${name}")
      mark_as_advanced(GDAL_CHECK_PACKAGE_${name}_NAMES)
    endif ()
    if (_GCP_TARGETS)
      set(GDAL_CHECK_PACKAGE_${name}_TARGETS "${_GCP_TARGETS}" CACHE STRING "Target name candidates for ${name}")
      mark_as_advanced(GDAL_CHECK_PACKAGE_${name}_TARGETS)
    endif ()
    if (GDAL_CHECK_PACKAGE_${name}_NAMES)
      find_package(${name} NAMES ${GDAL_CHECK_PACKAGE_${name}_NAMES} ${_find_package_args})
      gdal_check_package_target(${name} ${GDAL_CHECK_PACKAGE_${name}_TARGETS} REQUIRED)
      if (${name}_FOUND)
        get_filename_component(_find_dependency_args "${${name}_CONFIG}" NAME)
        string(REPLACE ";" " " _find_dependency_args "${name} ${_find_package_args} NAMES ${GDAL_CHECK_PACKAGE_${name}_NAMES} CONFIGS ${_find_dependency_args}")
      endif ()
    endif ()
    if (NOT ${name}_FOUND)
      find_package(${name} ${_find_package_args})
      if (${name}_FOUND)
        gdal_check_package_target(${name} ${GDAL_CHECK_PACKAGE_${name}_TARGETS})
      elseif (${key}_FOUND) # Some find modules do not set <Pkg>_FOUND
        gdal_check_package_target(${key} ${GDAL_CHECK_PACKAGE_${name}_TARGETS})
        set(${name}_FOUND "${key}_FOUND")
      endif ()
      if (${name}_FOUND)
        string(REPLACE ";" " " _find_dependency_args "${name} ${_find_package_args}")
      endif()
    endif ()
  endif ()
  if (${key}_FOUND OR ${name}_FOUND)
    if(_GCP_VERSION)

      if( "${name}" STREQUAL "TileDB" AND NOT DEFINED TileDB_VERSION)
        get_property(_dirs TARGET TileDB::tiledb_shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _dirs)
          set(TILEDB_VERSION_FILENAME "${_dir}/tiledb/tiledb_version.h")
          if(EXISTS ${TILEDB_VERSION_FILENAME})
            file(READ ${TILEDB_VERSION_FILENAME} _tiledb_version_contents)
            string(REGEX REPLACE "^.*TILEDB_VERSION_MAJOR +([0-9]+).*$" "\\1" TILEDB_VERSION_MAJOR "${_tiledb_version_contents}")
            string(REGEX REPLACE "^.*TILEDB_VERSION_MINOR +([0-9]+).*$" "\\1" TILEDB_VERSION_MINOR "${_tiledb_version_contents}")
            set(TileDB_VERSION "${TILEDB_VERSION_MAJOR}.${TILEDB_VERSION_MINOR}")
          endif()
        endforeach()
      endif()

      if (DEFINED ${name}_VERSION_STRING AND NOT DEFINED ${name}_VERSION)
          set(${name}_VERSION "${${name}_VERSION_STRING}")
      endif()

      if( "${${name}_VERSION}" STREQUAL "")
        message(WARNING "${name} has unknown version. Assuming it is at least matching the minimum version required of ${_GCP_VERSION}")
        set(HAVE_${key} ON)
      elseif( ${name}_VERSION VERSION_LESS ${_GCP_VERSION})
        message(WARNING "Ignoring ${name} because it is at version ${${name}_VERSION}, whereas the minimum version required is ${_GCP_VERSION}")
        set(HAVE_${key} OFF)
      else()
        set(HAVE_${key} ON)
      endif()
    else()
      set(HAVE_${key} ON)
    endif()
  else ()
    set(HAVE_${key} OFF)
  endif ()
  if (purpose STREQUAL "")

  else ()
    if (_GCP_RECOMMENDED)
      set_package_properties(
        ${name} PROPERTIES
        PURPOSE ${purpose}
        TYPE RECOMMENDED)
    else ()
      set_package_properties(${name} PROPERTIES PURPOSE ${purpose})
    endif ()
  endif ()

  if (_GCP_CAN_DISABLE OR _GCP_DISABLED_BY_DEFAULT)
    set(_gcpp_status ON)
    if (GDAL_USE_${key})
      if (NOT HAVE_${key})
        message(FATAL_ERROR "Configured to use ${key}, but not found")
      endif ()
    elseif (NOT GDAL_USE_EXTERNAL_LIBS)
      set(_gcpp_status OFF)
      if (HAVE_${key} AND NOT GDAL_USE_${key})
        message(STATUS
          "${key} has been found, but is disabled due to GDAL_USE_EXTERNAL_LIBS=OFF. Enable it by setting GDAL_USE_${key}=ON"
          )
        set(_find_dependency_args "")
      endif ()
    endif ()
    if (_gcpp_status AND _GCP_DISABLED_BY_DEFAULT)
      set(_gcpp_status OFF)
      if (HAVE_${key} AND NOT GDAL_USE_${key})
        message(STATUS "${key} has been found, but is disabled by default. Enable it by setting GDAL_USE_${key}=ON")
        set(_find_dependency_args "")
      endif ()
    endif ()
    cmake_dependent_option(GDAL_USE_${key} "Set ON to use ${key}" ${_gcpp_status} "HAVE_${key}" OFF)
  elseif (NOT _GCP_ALWAYS_ON_WHEN_FOUND)
    message(FATAL_ERROR "Programming error: missing CAN_DISABLE or DISABLED_BY_DEFAULT option for component ${name}")
  endif ()

  if(_find_dependency_args)
    string(REPLACE "\"" "\\\"" _find_dependency_args "${_find_dependency_args}")
    set(_find_dependency "find_dependency(${_find_dependency_args})\n")
  endif()
  if(NOT BUILD_SHARED_LIBS AND GDAL_USE_${key} AND _find_dependency)
    string(APPEND GDAL_IMPORT_DEPENDENCIES "${_find_dependency}")
  endif()
  unset(_find_dependency_args)
  unset(_find_dependency)
endmacro ()

function (split_libpath _lib)
  if (_lib)
    # split lib_line into -L and -l linker options
    get_filename_component(_path ${${_lib}} PATH)
    get_filename_component(_name ${${_lib}} NAME_WE)
    string(REGEX REPLACE "^lib" "" _name ${_name})
    set(${_lib} -L${_path} -l${_name})
  endif ()
endfunction ()

function (gdal_internal_library libname)
  set(_options REQUIRED)
  set(_oneValueArgs)
  set(_multiValueArgs)
  cmake_parse_arguments(_GIL "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if ("${GDAL_USE_INTERNAL_LIBS}" STREQUAL "ON")
      set(_default_value ON)
  elseif ("${GDAL_USE_INTERNAL_LIBS}" STREQUAL "OFF")
      set(_default_value OFF)
  elseif( GDAL_USE_${libname} )
      set(_default_value OFF)
  else()
      set(_default_value ON)
  endif()
  set(GDAL_USE_${libname}_INTERNAL
      ${_default_value}
      CACHE BOOL "Use internal ${libname} copy (if set to ON, has precedence over GDAL_USE_${libname})")
  if (_GIL_REQUIRED
      AND (NOT GDAL_USE_${libname})
      AND (NOT GDAL_USE_${libname}_INTERNAL))
    message(FATAL_ERROR "GDAL_USE_${libname} or GDAL_USE_${libname}_INTERNAL must be set to ON")
  endif ()
endfunction ()

# vim: ts=4 sw=4 sts=4 et
