# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
GdalDriverHelper
-----------------

  target_name should be as same as plugin name.
      name gdal_* as recognized as raster driver and
      name ogr_* as vector one.
  and lookup register function from filename.

  Symptoms add_gdal_driver( TARGET <target_name>
                            [SOURCES <source file> [<source file>[...]]]
                            BUILTIN | PLUGIN_CAPABLE | [PLUGIN_CAPABLE_IF <cond>]
                            [NO_DEPS]
                          )
           gdal_standard_includes(<target_name>)
           gdal_target_link_libraries(TARGET <target_name> LIBRARIES <library> [<library2> [..]])

  Drivers should specify one and only one of:
  - BUILTIN: the driver is built-in into library, and cannot be built as a plugin
  - PLUGIN_CAPABLE: the driver can be built as a plugin.
    This only happens if the GDAL_ENABLE_FRMT_{foo}_PLUGIN or OGR_ENABLE_{foo}_PLUGIN
    variable is set to ON.
    The default value of that variable is :
    - the value of GDAL_ENABLE_PLUGINS option when NO_DEPS is not specified (e.g ECW, HDF4, etc.)
    - the value of GDAL_ENABLE_PLUGINS_NO_DEPS option when NO_DEPS is specified
  - PLUGIN_CAPABLE_IF: similar to PLUGIN_CAPABLE,
    but extra conditions provided in <cond> (e.g "NOT GDAL_USE_LIBJSONC_INTERNAL") are needed

  The NO_DEPS option express that the driver has no non-core external dependencies.

  There are several examples to show how to write build cmake script.

 ex.1 Driver which is referenced by other drivers
      Such driver should built-in into library to resolve reference.

   add_gdal_driver(TARGET gdal_iso8211 SOURCES iso8211.cpp BUILTIN)

 ex.2 Driver that refer other driver as dependency
      Please do not specify LIBRARIES for linking target for other driver,
      That should be built into libgdal.

   add_gdal_driver(TARGET gdal_ADRG SOURCES foo.cpp BUILTIN)
   target_include_directories(gdal_ADRG PRIVATE $<TARGET_PROPERTY:iso8211,SOURCE_DIR>)

 ex.3  Driver which is depend on some external libraries
       These definitions are detected in cmake/macro/CheckDependentLibraries.cmake
       If you cannot find your favorite library in the macro, please add it to
       CheckDependentLibraries.cmake.

   add_gdal_driver(TARGET    gdal_WEBP
                   SOURCES   gdal_webp.c gdal_webp.h PLUGIN_CAPABLE)
   gdal_standard_includes(gdal_WEBP)
   target_include_directories(gdal_WEBP PRIVATE ${WEBP_INCLUDE_DIRS} ${TIFF_INCLUDE_DIRS})
   gdal_target_link_libraries(TARGET gdal_WEBP LIBRARIES ${WEBP_LIBRARIES} ${TIFF_LIBRARIES})


 ex.4  Driver which is depend on internal bundled thirdparty libraries
       To refer thirdparty library dev files, pls use '$<TARGET_PROPERTY:(target_library),SOURCE_DIR>'
       cmake directive.
       You may use 'IF(GDAL_USE_SOME_LIBRARY_INTERNAL)...ELSE()...ENDIF()' cmake directive too.

   add_gdal_driver(TARGET gdal_CALS
                   SOURCES calsdataset.cpp BUILTIN)
   gdal_standard_includes(gdal_CALS)
   gdal_include_directories(gdal_CALS PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)

#]=======================================================================]

function(add_gdal_driver)
    set(_options BUILTIN PLUGIN_CAPABLE NO_DEPS STRONG_CXX_WFLAGS CXX_WFLAGS_EFFCXX NO_CXX_WFLAGS)
    set(_oneValueArgs TARGET DESCRIPTION DEF PLUGIN_CAPABLE_IF DRIVER_NAME_OPTION)
    set(_multiValueArgs SOURCES)
    cmake_parse_arguments(_DRIVER "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    # Check mandatory arguments
    if (NOT _DRIVER_TARGET)
        message(FATAL_ERROR "ADD_GDAL_DRIVER(): TARGET is a mandatory argument.")
    endif ()
    if (NOT _DRIVER_SOURCES)
        message(FATAL_ERROR "ADD_GDAL_DRIVER(): SOURCES is a mandatory argument.")
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
    string(TOUPPER ${_FORMAT} _KEY)

    if ((NOT _DRIVER_PLUGIN_CAPABLE) AND (NOT _DRIVER_BUILTIN) AND (NOT _DRIVER_PLUGIN_CAPABLE_IF))
        message(FATAL_ERROR "Driver ${_DRIVER_TARGET} should declare BUILTIN, PLUGIN_CAPABLE or PLUGIN_CAPABLE_IF")
    endif()

    # Determine whether plugin or built-in
    set(_DRIVER_PLUGIN_BUILD OFF)
    set(_COND)
    if (_DRIVER_PLUGIN_CAPABLE_IF)
        set(_COND ${_DRIVER_PLUGIN_CAPABLE_IF})
    endif()
    if (_DRIVER_PLUGIN_CAPABLE OR _DRIVER_PLUGIN_CAPABLE_IF)
        set(_INITIAL_VALUE OFF)
        if( GDAL_ENABLE_PLUGINS AND NOT _DRIVER_NO_DEPS )
            set(_INITIAL_VALUE ON)
        elseif( GDAL_ENABLE_PLUGINS_NO_DEPS AND _DRIVER_NO_DEPS )
            set(_INITIAL_VALUE ON)
        endif()
        if( _DRIVER_DRIVER_NAME_OPTION )
            string(TOUPPER ${_DRIVER_DRIVER_NAME_OPTION} _KEY)
        endif()
        if( IS_OGR EQUAL -1) # raster
            set(_enable_plugin_var GDAL_ENABLE_FRMT_${_KEY})
            if( NOT DEFINED ${_enable_plugin_var} )
                message(FATAL_ERROR "Option ${_enable_plugin_var} does not exist")
            endif()
            cmake_dependent_option(${_enable_plugin_var}_PLUGIN "Set ON to build GDAL ${_KEY} driver as plugin"
                                   ${_INITIAL_VALUE}
                                   "${_enable_plugin_var};${_COND}" OFF)
            if( ${_enable_plugin_var}_PLUGIN )
                set(_DRIVER_PLUGIN_BUILD ON)
            endif()
        else()
            set(_enable_plugin_var OGR_ENABLE_${_KEY})
            if( NOT DEFINED ${_enable_plugin_var} )
                message(FATAL_ERROR "Option ${_enable_plugin_var} does not exist")
            endif()
            cmake_dependent_option(${_enable_plugin_var}_PLUGIN "Set ON to build OGR ${_KEY} driver as plugin"
                                   ${_INITIAL_VALUE}
                                   "${_enable_plugin_var};${_COND}" OFF)
            if( ${_enable_plugin_var}_PLUGIN )
                set(_DRIVER_PLUGIN_BUILD ON)
            endif()
        endif()
    endif()

    # target configuration
    if (_DRIVER_PLUGIN_BUILD)
        # target become *.so *.dll or *.dylib
        add_library(${_DRIVER_TARGET} MODULE ${_DRIVER_SOURCES})
        get_target_property(PLUGIN_OUTPUT_DIR ${GDAL_LIB_TARGET_NAME} PLUGIN_OUTPUT_DIR)
        set_target_properties(${_DRIVER_TARGET}
                              PROPERTIES
                              PREFIX ""
                              LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
                              SKIP_BUILD_RPATH YES
                              )
        # The following doesn't work: we have to manually tweak will install_name_tool
        #if (GDAL_ENABLE_MACOSX_FRAMEWORK)
        #    set_target_properties(${_DRIVER_TARGET}
        #                          PROPERTIES
        #                          INSTALL_RPATH "@loader_path/../../../..")
        #endif()
        target_link_libraries(${_DRIVER_TARGET} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
        install(FILES $<TARGET_LINKER_FILE:${_DRIVER_TARGET}> DESTINATION ${INSTALL_PLUGIN_DIR}
                RENAME "${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}" NAMELINK_SKIP)
        if (GDAL_ENABLE_MACOSX_FRAMEWORK)
            file(RELATIVE_PATH relDir
                 ${CMAKE_CURRENT_BINARY_DIR}/${INSTALL_PLUGIN_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR}/${FRAMEWORK_DESTINATION})
            install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"@loader_path/${relDir}\" \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGIN_DIR}/${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}\")")
        endif()
        set_property(GLOBAL APPEND PROPERTY PLUGIN_MODULES ${_DRIVER_TARGET})
    else ()
        add_library(${_DRIVER_TARGET} OBJECT ${_DRIVER_SOURCES})
        set_property(TARGET ${_DRIVER_TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
        target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:${_DRIVER_TARGET}>)
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
                target_compile_definitions(ogrsf_frmts PRIVATE -D${_KEY}_ENABLED)
            endif ()
        endif ()
    endif ()
    if (_DRIVER_CXX_WFLAGS_EFFCXX)
        target_compile_options(${_DRIVER_TARGET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS} ${WFLAG_EFFCXX}>)
    elseif (_DRIVER_STRONG_CXX_WFLAGS)
        target_compile_options(${_DRIVER_TARGET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX}>)
    elseif( NOT _DRIVER_NO_CXX_WFLAGS )
        target_compile_options(${_DRIVER_TARGET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS}>)
    endif()
    target_compile_options(${_DRIVER_TARGET} PRIVATE $<$<COMPILE_LANGUAGE:C>:${GDAL_C_WARNING_FLAGS}>)
    add_dependencies(${_DRIVER_TARGET} generate_gdal_version_h)
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
        gdal_add_private_link_libraries(${_DRIVER_LIBRARIES})
    endif ()
endfunction()

macro(gdal_driver_standard_includes _TARGET)
    include(GdalStandardIncludes)
    gdal_standard_includes(${_TARGET})
endmacro()

#  Macro for including  driver directories.
#  Following macro should use only in the directories:
#
#  ogr/ogrsf_frmts/
#  frmts/
#

include(CMakeDependentOption)

macro(check_depend_condition depends)
    foreach(_dep IN ITEMS ${depends})
        if( "${_dep}" MATCHES "GDAL_ENABLE_FRMT_" OR "${_dep}" MATCHES "OGR_ENABLE_")
            if(NOT DEFINED "${_dep}")
                message(FATAL_ERROR "Condition ${depends} refers to variable ${_dep} which is not defined")
            endif()
        endif()
    endforeach()
endmacro()

# gdal_dependent_format(format desc depend) do followings:
# - add subdirectory 'format'
# - define option "GDAL_ENABLE_FRMT_NAME" then set to default OFF/ON
# - when enabled, add definition"-DFRMT_format"
# - when dependency specified by depend fails, force OFF
macro(gdal_dependent_format format desc depends)
    set(_options SKIP_ADD_SUBDIRECTORY)
    set(_oneValueArgs DRIVER_NAME_OPTION)
    set(_multiValueArgs)
    cmake_parse_arguments(_GDF "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if( _GDF_DRIVER_NAME_OPTION )
        string(TOUPPER ${_GDF_DRIVER_NAME_OPTION} key)
    else()
        string(TOUPPER ${format} key)
    endif()
    check_depend_condition(${depends})
    cmake_dependent_option(GDAL_ENABLE_FRMT_${key} "Set ON to build ${desc} format" ${GDAL_BUILD_OPTIONAL_DRIVERS}
                           "${depends}" OFF)
    add_feature_info(gdal_${key} GDAL_ENABLE_FRMT_${key} "${desc}")
    if (GDAL_ENABLE_FRMT_${key} AND NOT _GDF_SKIP_ADD_SUBDIRECTORY)
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
    set(_options)
    set(_oneValueArgs DRIVER_NAME_OPTION)
    set(_multiValueArgs)
    cmake_parse_arguments(_GOF "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if( _GOF_DRIVER_NAME_OPTION )
        string(TOUPPER ${_GOF_DRIVER_NAME_OPTION} key)
    else()
        string(TOUPPER ${format} key)
    endif()
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
    check_depend_condition(${depend})
    if( NOT("${key}" STREQUAL "GPKG" OR "${key}" STREQUAL "SQLITE" OR "${key}" STREQUAL "AVC") )
        cmake_dependent_option(OGR_ENABLE_${key} "Set ON to build OGR ${desc} driver" ${OGR_BUILD_OPTIONAL_DRIVERS}
                               "${depend}" OFF)
    endif()
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

