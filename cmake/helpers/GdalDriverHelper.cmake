# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file gdal/LICENSE.TXT.

#[=======================================================================[.rst:
GdalDriverHelper
-----------------

  target_name should be as same as plugin name.
      name gdal_* as recognized as raster driver and
      name ogr_* as vector one.
  and lookup register function from filename.

  Symptoms add_gdal_driver( TARGET <target_name>
                            [SOURCES <source file> [<source file>[...]]]
                            [BUILTIN]
                          )
           gdal_standard_includes(<target_name>)
           gdal_target_link_libraries(TARGET <target_name> LIBRARIES <library> [<library2> [..]])



  All in one macro; not recommended.

  Symptoms gdal_driver( TARGET <target_name>
                        [SOURCES <source file> [<source file>[...]]]
                        [INCLUDES <include_dir> [<include dir2> [...]]]
                        [LIBRARIES <library1> [<library2> [...]][
                        [DEFINITIONS -DFOO=1 [-DBOO [...]]]
                        [BUILTIN]
          )

  All driver which is not specify 'BUILTIN' beocmes PLUGIN when
  configuration ENABLE_PLUGIN = true.

  There aree several examples to show how to write build cmake script.

 ex.1 Driver which is referrenced by other drivers
      Such driver should built-in into library to resolve reference.
      Please use 'FORCE_BUILTIN' option keyword which indicate to link it into libgdal.so.

   add_gdal_driver(TARGET gdal_iso8211 SOURCES iso8211.cpp BUILTIN)

 ex.2 Driver that refer other driver as dependency
      Please do not specify LIBRARIES for linking target for other driver,
      That should be bulit into libgdal.

   add_gdal_driver(TARGET gdal_ADRG SOURCES foo.cpp)
   target_include_directories(gdal_ADRG PRIVATE $<TARGET_PROPERTY:iso8211,SOURCE_DIR>)

 ex.3  Driver which is depend on some external libraries
       These definitions are detected in cmake/macro/CheckDependentLibraries.cmake
       If you cannot find your favorite library in the macro, please add it to
       CheckDependentLibraries.cmake.

   add_gdal_driver(TARGET    gdal_WEBP
               SOURCES   gdal_webp.c gdal_webp.h)
   gdal_standard_includes(gdal_WEBP)
   target_include_directories(gdal_WEBP PRIVATE ${WEBP_INCLUDE_DIRS} ${TIFF_INCLUDE_DIRS})
   gdal_target_link_libraries(TARGET gdal_WEBP LIBRARIES ${WEBP_LIBRARIES} ${TIFF_LIBRARIES})


 ex.4  Driver which is depend on internal bundled thirdparty libraries
       To refer thirdparty library dev files, pls use '$<TARGET_PROPERTY:(target_library),SOURCE_DIR>'
       cmake directive.
       You may use 'IF(GDAL_USE_SOME_LIBRARY_INTERNAL)...ELSE()...ENDIF()' cmake directive too.

   add_gdal_driver(TARGET gdal_CALS
               SOURCES calsdataset.cpp)
   gdal_standard_includes(gdal_CALS)
   gdal_include_directories(gdal_CALS PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)

#]=======================================================================]

function(add_gdal_driver)
    set(_options BUILTIN PLUGIN)
    set(_oneValueArgs TARGET DESCRIPTION DEF)
    set(_multiValueArgs SOURCES)
    cmake_parse_arguments(_DRIVER "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    # Check mandatory arguments
    if (NOT _DRIVER_TARGET)
        message(FATAL_ERROR "ADD_GDAL_DRIVER(): TARGET is a mandatory argument.")
    endif ()
    if (NOT _DRIVER_SOURCES)
        message(FATAL_ERROR "ADD_GDAL_DRIVER(): SOURCES is a mandatory argument.")
    endif ()
    # Determine whether plugin or built-in
    if (_DRIVER_PLUGIN)
        # When specified PLUGIN, always build as plugin
        set(_DRIVER_PLUGIN_BUILD TRUE)
    elseif ((NOT GDAL_ENABLE_PLUGIN) OR _DRIVER_BUILTIN)
        set(_DRIVER_PLUGIN_BUILD FALSE)
    else ()
        set(_DRIVER_PLUGIN_BUILD TRUE)
    endif ()
    # Set *_FORMATS properties for summary and gdal_config utility
    string(FIND "${_DRIVER_TARGET}" "ogr" IS_OGR)
    if (IS_OGR EQUAL -1) # raster
        string(REPLACE "gdal_" "" _FORMAT ${_DRIVER_TARGET})
        set_property(GLOBAL APPEND PROPERTY GDAL_FORMATS ${_FORMAT})
    else () # vector
        string(REPLACE "ogr_" "" _FORMAT ${_DRIVER_TARGET})
        set_property(GLOBAL APPEND PROPERTY OGR_FORMATS ${_FORMAT})
    endif ()
    # target configuration
    if (_DRIVER_PLUGIN_BUILD)
        # target become *.so *.dll or *.dylib
        add_library(${_DRIVER_TARGET} MODULE ${_DRIVER_SOURCES})
        get_target_property(PLUGIN_OUTPUT_DIR gdal PLUGIN_OUTPUT_DIR)
        set_target_properties(${_DRIVER_TARGET}
                              PROPERTIES
                              PREFIX ""
                              LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
                              )
        target_link_libraries(${_DRIVER_TARGET} PRIVATE $<TARGET_NAME:gdal>)
        install(FILES $<TARGET_LINKER_FILE:${_DRIVER_TARGET}> DESTINATION ${INSTALL_PLUGIN_DIR}
                RENAME "${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}" NAMELINK_SKIP)
        set_property(GLOBAL APPEND PROPERTY PLUGIN_MODULES ${_DRIVER_TARGET})
    else ()
        add_library(${_DRIVER_TARGET} OBJECT ${_DRIVER_SOURCES})
        set_property(TARGET ${_DRIVER_TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
        target_sources(gdal PRIVATE $<TARGET_OBJECTS:${_DRIVER_TARGET}>)
        if (_DRIVER_DEF)
            if (IS_OGR EQUAL -1) # raster
                target_compile_definitions(gdal_frmts PRIVATE -D${_DRIVER_DEF})
            else ()
                target_compile_definitions(ogrsf_frmts PRIVATE -D${_DRIVER_DEF})
            endif ()
        else ()
            if (IS_OGR EQUAL -1) # raster
                string(TOLOWER ${_DRIVER_TARGET} _FORMAT)
                string(REPLACE "gdal" "FRMT" _DEF ${_FORMAT})
                target_compile_definitions(gdal_frmts PRIVATE -D${_DEF})
            else () # vector
                string(REPLACE "ogr_" "" _FORMAT ${_DRIVER_TARGET})
                string(TOUPPER ${_FORMAT} _KEY)
                target_compile_definitions(ogrsf_frmts PRIVATE -D${_KEY}_ENABLED)
            endif ()
        endif ()
    endif ()
endfunction()

# Detect whether driver is built as PLUGIN or not.
function(is_plugin _result _target)
    get_property(_PLUGIN_MODULES GLOBAL PROPERTY PLUGIN_MODULES)
    list(FIND _PLUGIN_MODULES ${_target} _IS_DRIVER_PLUGIN)
    if (_IS_DRIVER_PLUGIN EQUAL -1)
        set(${_result} FALSE PARENT_SCOPE)
    else ()
        set(${_result} TRUE PARENT_SCOPE)
    endif ()
endfunction()

function(gdal_target_interfaces _TARGET)
    foreach (_LIB IN ITEMS ${ARGN})
        if (TARGET ${_LIB})
            get_property(_res TARGET ${_LIB} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
            if (_res)
                target_include_directories(${_TARGET} PRIVATE ${_res})
            endif ()
            get_property(_res TARGET ${_LIB} PROPERTY INTERFACE_COMPILE_DEFINITIONS)
            if (_res)
                target_compile_definitions(${_TARGET} PRIVATE ${_res})
            endif ()
            get_property(_res TARGET ${_LIB} PROPERTY INTERFACE_COMPILE_OPTIONS)
            if (_res)
                target_compile_options(${_TARGET} PRIVATE ${_res})
            endif ()
        endif ()
    endforeach ()
endfunction()

function(gdal_target_link_libraries)
    set(_oneValueArgs TARGET)
    set(_multiValueArgs LIBRARIES)
    cmake_parse_arguments(_DRIVER "" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if (NOT _DRIVER_TARGET)
        message(FATAL_ERROR "GDAL_TARGET_LINK_LIBRARIES(): TARGET is a mandatory argument.")
    endif ()
    if (NOT _DRIVER_LIBRARIES)
        message(FATAL_ERROR "GDAL_TARGET_LINK_LIBRARIES(): LIBRARIES is a mandatory argument.")
    endif ()
    is_plugin(RES ${_DRIVER_TARGET})
    if (RES)
        target_link_libraries(${_DRIVER_TARGET} PRIVATE ${_DRIVER_LIBRARIES})
    else ()
        gdal_target_interfaces(${_DRIVER_TARGET} ${_DRIVER_LIBRARIES})
        target_link_libraries(GDAL_LINK_LIBRARY INTERFACE ${_DRIVER_LIBRARIES})
    endif ()
endfunction()

macro(gdal_driver_standard_includes _TARGET)
    include(GdalStandardIncludes)
    gdal_standard_includes(${_TARGET})
endmacro()

#  Macro for including  driver directories.
#  Following macro should use only in the directories:
#
#  gdal/ogr/ogrsf_frmts/
#  gdal/frmts/
#

include(CMakeDependentOption)

# gdal_dependent_format(format desc depend) do followings:
# - add subdirectory 'format'
# - define option "GDAL_ENABLE_FRMT_NAME" then set to default OFF/ON
# - when enabled, add definition"-DFRMT_format"
# - when dependency specified by depend fails, force OFF
macro(gdal_dependent_format format desc depends)
    string(TOUPPER ${format} key)
    cmake_dependent_option(GDAL_ENABLE_FRMT_${key} "Set ON to build ${desc} format" ON
                           "${depends}" OFF)
    add_feature_info(gdal_${key} GDAL_ENABLE_FRMT_${key} "${desc}")
    if (GDAL_ENABLE_FRMT_${key})
        add_subdirectory(${format})
    endif ()
endmacro()

macro(gdal_format format desc)
    string(TOUPPER ${format} key desc)
    set(GDAL_ENABLE_FRMT_${key} ON CACHE BOOL "" FORCE)
    add_feature_info(gdal_${key} GDAL_ENABLE_FRMT_${key} "${desc}")
    add_subdirectory(${format})
endmacro()

macro(gdal_optional_format format desc)
    string(TOUPPER ${format} key)
    option(GDAL_ENABLE_FRMT_${key} "Set ON to build ${desc} format" ${GDAL_BUILD_OPTIONAL_DRIVERS})
    add_feature_info(gdal_${key} GDAL_ENABLE_FRMT_${key} "${desc}")
    if (GDAL_ENABLE_FRMT_${key})
        add_subdirectory(${format})
    endif ()
endmacro()

# ogr_dependent_driver(NAME desc depend) do followings:
# - define option "OGR_ENABLE_<name>" with default OFF
# - add subdirectory 'name'
# - when dependency specified by depend fails, force OFF

macro(ogr_dependent_driver name desc depend)
    string(TOUPPER ${name} key)
    cmake_dependent_option(OGR_ENABLE_${key} "Set ON to build OGR ${desc} driver" ON
                           "${depend}" OFF)
    add_feature_info(ogr_${key} OGR_ENABLE_${key} "${desc}")
    if (OGR_ENABLE_${key})
        add_subdirectory(${name})
    endif ()
endmacro()

# ogr_optional_driver(name desc) do followings:
# - define option "OGR_ENABLE_<name>" with default OFF
# - add subdirectory 'name' when enabled
macro(ogr_optional_driver name desc)
    string(TOUPPER ${name} key)
    option(OGR_ENABLE_${key} "Set ON to build OGR ${desc} driver" ${OGR_BUILD_OPTIONAL_DRIVERS})
    add_feature_info(ogr_${key} OGR_ENABLE_${key} "${desc}")
    if (OGR_ENABLE_${key})
        add_subdirectory(${name})
    endif ()
endmacro()

# ogr_default_driver(name desc)
# - set "OGR_ENABLE_<name>" is ON but configurable.
# - add subdirectory "name"
macro(ogr_default_driver name desc)
    string(TOUPPER ${name} key)
    set(OGR_ENABLE_${key} ON CACHE BOOL "${desc}" FORCE)
    add_feature_info(ogr_${key} OGR_ENABLE_${key} "${desc}")
    add_subdirectory(${name})
endmacro()
macro(ogr_default_driver2 name key desc)
    set(OGR_ENABLE_${key} ON CACHE BOOL "${desc}" FORCE)
    add_feature_info(ogr_${key} OGR_ENABLE_${key} "${desc}")
    add_subdirectory(${name})
endmacro()

