# Distributed under the GDAL/OGR MIT style License.  See accompanying
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
                            [CORE_SOURCES <source file> [<source file>[...]]]
                            NO_SHARED_SYMBOL_WITH_CORE
                            BUILTIN | PLUGIN_CAPABLE | [PLUGIN_CAPABLE_IF <cond>]
                            [NO_DEPS]
                          )
           gdal_standard_includes(<target_name>)
           gdal_target_link_libraries(<target_name> PRIVATE <library> [<library2> [..]])

  Drivers should specify one and only one of:
  - BUILTIN: the driver is built-in into library, and cannot be built as a plugin
  - PLUGIN_CAPABLE: the driver can be built as a plugin.
    This only happens if the GDAL_ENABLE_DRIVER_{foo}_PLUGIN or OGR_ENABLE_DRIVER_{foo}_PLUGIN
    variable is set to ON.
    The default value of that variable is :
    - the value of GDAL_ENABLE_PLUGINS option when NO_DEPS is not specified (e.g ECW, HDF4, etc.)
    - the value of GDAL_ENABLE_PLUGINS_NO_DEPS option when NO_DEPS is specified
  - PLUGIN_CAPABLE_IF: similar to PLUGIN_CAPABLE,
    but extra conditions provided in <cond> (e.g "NOT GDAL_USE_JSONC_INTERNAL") are needed

  The NO_DEPS option express that the driver has no non-core external dependencies.

  The CORE_SOURCES option points to a list of files that are used for deferred plugin
  loading (cf RFC 96 https://gdal.org/development/rfc/rfc96_deferred_plugin_loading.html),
  when the driver is built as a plugin.
  Those files are build in libgdal so it has access to plugin metadata and the
  identification method, and are typically exported so that the driver can also
  access them.
  However it has been found that this resulted in the inability of building a
  plugin for a libgdal that hadn't been built with any form of support for it,
  which is undesirable.
  Starting with GDAL 3.9.1, NO_SHARED_SYMBOL_WITH_CORE must also be declared
  when CORE_SOURCES is declared, so that the files pointed by CORE_SOURCES
  are built twice: once in libgdal with a "GDAL_core_" prefix, and another time
  in the plugin itself with a "GDAL_driver_" prefix, by using the
  PLUGIN_SYMBOL_NAME() macro of gdal_priv.h

  Example in ogrocidrivercore.h:

        #define OGROCIDriverIdentify \
           PLUGIN_SYMBOL_NAME(OGROCIDriverIdentify)
        #define OGROCIDriverSetCommonMetadata \
           PLUGIN_SYMBOL_NAME(OGROCIDriverSetCommonMetadata)

        int OGROCIDriverIdentify(GDALOpenInfo *poOpenInfo);

        void OGROCIDriverSetCommonMetadata(GDALDriver *poDriver);



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
   gdal_target_link_libraries(gdal_WEBP PRIVATE WEBP::WebP)


 ex.4  Driver which is depend on internal bundled thirdparty libraries
       To refer thirdparty library dev files, pls use '$<TARGET_PROPERTY:(target_library),SOURCE_DIR>'
       cmake directive.
       You may use 'IF(GDAL_USE_SOME_LIBRARY_INTERNAL)...ELSE()...ENDIF()' cmake directive too.

   add_gdal_driver(TARGET gdal_CALS
                   SOURCES calsdataset.cpp BUILTIN)
   gdal_standard_includes(gdal_CALS)
   gdal_include_directories(gdal_CALS PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)

#]=======================================================================]

function(_set_driver_core_sources _KEY _DRIVER_TARGET)

    add_library(${_DRIVER_TARGET}_core OBJECT ${ARGN})
    set_property(TARGET ${_DRIVER_TARGET}_core PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
    target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:${_DRIVER_TARGET}_core>)
    target_compile_definitions(${_DRIVER_TARGET}_core PRIVATE "-DPLUGIN_FILENAME=\"${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
    string(FIND "${_DRIVER_TARGET}" "ogr" IS_OGR)
    if (IS_OGR EQUAL -1) # raster
        set(_var_PLUGIN_INSTALLATION_MESSAGE "GDAL_DRIVER_${_KEY}_PLUGIN_INSTALLATION_MESSAGE")
    else()
        set(_var_PLUGIN_INSTALLATION_MESSAGE "OGR_DRIVER_${_KEY}_PLUGIN_INSTALLATION_MESSAGE")
    endif()
    if (DEFINED ${_var_PLUGIN_INSTALLATION_MESSAGE})
        target_compile_definitions(${_DRIVER_TARGET}_core PRIVATE "-DPLUGIN_INSTALLATION_MESSAGE=\"${${_var_PLUGIN_INSTALLATION_MESSAGE}}\"")
    endif()
    gdal_standard_includes(${_DRIVER_TARGET}_core)
    add_dependencies(${_DRIVER_TARGET}_core generate_gdal_version_h)

    target_compile_definitions(gdal_frmts PRIVATE -DDEFERRED_${_KEY}_DRIVER)

endfunction()

function(add_gdal_driver)
    set(_options BUILTIN PLUGIN_CAPABLE NO_DEPS STRONG_CXX_WFLAGS CXX_WFLAGS_EFFCXX NO_CXX_WFLAGS NO_SHARED_SYMBOL_WITH_CORE)
    set(_oneValueArgs TARGET DESCRIPTION DEF PLUGIN_CAPABLE_IF DRIVER_NAME_OPTION)
    set(_multiValueArgs SOURCES CORE_SOURCES)
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

    get_target_property(PLUGIN_OUTPUT_DIR ${GDAL_LIB_TARGET_NAME} PLUGIN_OUTPUT_DIR)

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
            set(_plugin_var_prefix GDAL)
        else()
            set(_plugin_var_prefix OGR)
        endif()

        set(_enable_plugin_var ${_plugin_var_prefix}_ENABLE_DRIVER_${_KEY})
        if( NOT DEFINED ${_enable_plugin_var} )
            message(FATAL_ERROR "Option ${_enable_plugin_var} does not exist")
        endif()

        if (DEFINED ${_enable_plugin_var}_PLUGIN AND DEFINED _DRIVER_PLUGIN_CAPABLE_IF)
          if (${${_enable_plugin_var}_PLUGIN})
            set(_COND_WITH_AND "")
            foreach(_c IN LISTS _COND)
                if(_COND_WITH_AND)
                    set(_COND_WITH_AND "${_COND_WITH_AND}) AND (${_c}")
                else()
                    set(_COND_WITH_AND "(${_c}")
                endif()
            endforeach()
            set(_COND_WITH_AND "${_COND_WITH_AND})")

            file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/check_plugin_conditions.cmake "
              if (NOT (${_COND_WITH_AND}))
                message(FATAL_ERROR \"${_enable_plugin_var}_PLUGIN required, but condition ${_COND} not met\")
              endif()"
            )

            include(${CMAKE_CURRENT_BINARY_DIR}/check_plugin_conditions.cmake)
          endif()
        endif()

        cmake_dependent_option(${_enable_plugin_var}_PLUGIN "Set ON to build ${_plugin_var_prefix} ${_KEY} driver as plugin"
                               ${_INITIAL_VALUE}
                               "${_enable_plugin_var};${_COND}" OFF)

        if( ${_enable_plugin_var}_PLUGIN )
            set(_DRIVER_PLUGIN_BUILD ON)
        endif()

        # If the GDAL/OGR_ENABLE_DRIVER_xxx_PLUGIN value has changed from its previous
        # value, and is now to OFF, make sure to clean stale plugins.
        if( ${_enable_plugin_var}_PLUGIN_OLD_VAL AND
            NOT ${_enable_plugin_var}_PLUGIN )
            foreach (_build_type IN ITEMS "" "Release/" "Debug/")
                set(_plugin_filename "${PLUGIN_OUTPUT_DIR}/${_build_type}${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}")
                if( EXISTS "${_plugin_filename}" )
                    message(STATUS "**Removing stale plugin**: ${_plugin_filename}")
                    file(REMOVE "${_plugin_filename}")
                endif()
            endforeach()
        endif()

        # Save new value of GDAL/OGR_ENABLE_DRIVER_xxx_PLUGIN
        set(${_enable_plugin_var}_PLUGIN_OLD_VAL ${${_enable_plugin_var}_PLUGIN} CACHE INTERNAL
            "Old value of option ${_enable_plugin_var}_PLUGIN")

        # If a driver is built in core libgdal, at install time, remove
        # potentiall corresponding stale installed plugin.
        if(NOT _DRIVER_PLUGIN_BUILD)
            install(CODE
                "
                set(_tmp \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGIN_DIR}/${_DRIVER_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}\")
                if( EXISTS \"\${_tmp}\")
                    message(STATUS \"**Removing stale plugin**: \${_tmp}\")
                    file(REMOVE \"\${_tmp}\")
                endif()
                ")
        endif()
    endif()

    if ((GDAL_REGISTER_DRIVER_${_KEY}_FOR_LATER_PLUGIN OR
         OGR_REGISTER_DRIVER_${_KEY}_FOR_LATER_PLUGIN) AND
         NOT GDAL_ENABLE_DRIVER_${_KEY} AND
         NOT OGR_ENABLE_DRIVER_${_KEY})
        if (NOT _DRIVER_CORE_SOURCES)
            if (GDAL_REGISTER_DRIVER_${_KEY}_FOR_LATER_PLUGIN)
                message(FATAL_ERROR "GDAL_REGISTER_DRIVER_${_KEY}_FOR_LATER_PLUGIN is set but that driver has no deferred plugin capabilities")
            else()
                message(FATAL_ERROR "OGR_REGISTER_DRIVER_${_KEY}_FOR_LATER_PLUGIN is set but that driver has no deferred plugin capabilities")
            endif()
        endif()

        _set_driver_core_sources(${_KEY} ${_DRIVER_TARGET} ${_DRIVER_CORE_SOURCES})

        return()
    endif()

    # target configuration
    if (_DRIVER_PLUGIN_BUILD)
        # target become *.so *.dll or *.dylib
        add_library(${_DRIVER_TARGET} MODULE ${_DRIVER_SOURCES})
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

        if (_DRIVER_CORE_SOURCES)
            if (_DRIVER_NO_SHARED_SYMBOL_WITH_CORE)
                foreach(f IN LISTS _DRIVER_CORE_SOURCES)
                    # Create a separate source file, to make sure we get a different object file
                    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/for_driver_${f}" "#include \"${CMAKE_CURRENT_SOURCE_DIR}/${f}\"")
                    target_sources(${_DRIVER_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/for_driver_${f}")
                endforeach()
            else()
                message(FATAL_ERROR "Driver ${_DRIVER_TARGET} should declare DRIVER_NO_SHARED_SYMBOL_WITH_CORE")
            endif()
            _set_driver_core_sources(${_KEY} ${_DRIVER_TARGET} ${_DRIVER_CORE_SOURCES})
        endif ()

    else ()
        add_library(${_DRIVER_TARGET} OBJECT ${_DRIVER_SOURCES} ${_DRIVER_CORE_SOURCES})
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
            get_property(_res TARGET ${_LIB} PROPERTY INTERFACE_LINK_LIBRARIES)
            if (_res)
                gdal_target_interfaces(${_TARGET} ${_res})
            endif ()
        endif ()
    endforeach ()
endfunction()

# To use a vendorized/internal library
function(gdal_add_vendored_lib _TARGET)
    foreach (_LIB IN ITEMS ${ARGN})
        if (TARGET ${_LIB})
            get_property(_res TARGET ${_LIB} PROPERTY SOURCE_DIR)
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

function(gdal_target_link_libraries target)
    set(_oneValueArgs)
    set(_multiValueArgs PRIVATE)
    cmake_parse_arguments(_DRIVER "" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if (NOT _DRIVER_PRIVATE)
        message(FATAL_ERROR "gdal_target_link_libraries(): PRIVATE is a mandatory argument.")
    endif ()
    is_plugin(RES ${target})
    if (RES)
        target_link_libraries(${target} PRIVATE ${_DRIVER_PRIVATE})
        foreach (_LIB IN ITEMS ${_DRIVER_PRIVATE})
            if (TARGET ${_LIB})
                get_property(_res TARGET ${_LIB} PROPERTY INTERFACE_COMPILE_DEFINITIONS)
                if (_res)
                    target_compile_definitions(${target} PRIVATE ${_res})
                endif ()
            endif()
        endforeach()
    else ()
        gdal_target_interfaces(${target} ${_DRIVER_PRIVATE})
        gdal_add_private_link_libraries(${_DRIVER_PRIVATE})
    endif ()

    # For debugging purposes
    if(SHOW_DEPS_PER_TARGET)
        foreach (_LIB IN ITEMS ${_DRIVER_PRIVATE})
            if (TARGET ${_LIB})
                include(GdalGenerateConfig)
                gdal_get_lflags(_libs ${_LIB})
                message(STATUS "Target ${target}: links against ${_libs} (target ${_LIB})")
            else()
                message(STATUS "Target ${target}: links against ${_LIB}")
            endif()
        endforeach()
    endif()
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

macro(check_depend_condition variable depends)
    foreach(_dep IN ITEMS ${depends})
        if( "${_dep}" MATCHES "GDAL_ENABLE_DRIVER_" OR "${_dep}" MATCHES "OGR_ENABLE_DRIVER_")
            if(NOT DEFINED "${_dep}")
                message(FATAL_ERROR "Condition ${depends} refers to variable ${_dep} which is not defined")
            endif()
        endif()
    endforeach()

    if(${variable})
        foreach(_dep IN ITEMS ${depends})
            cmake_dependent_option(TO_BE_REMOVED "" ON "${_dep}" OFF)
            if(NOT ${TO_BE_REMOVED})
                unset(TO_BE_REMOVED CACHE)
                if (NOT GDAL_IGNORE_FAILED_CONDITIONS)
                    set(ERROR_MSG_EXTRA_HINT)
                    if( "${_dep}" MATCHES "OGR_ENABLE_DRIVER_" AND NOT ${OGR_BUILD_OPTIONAL_DRIVERS})
                        set(ERROR_MSG_EXTRA_HINT ", presumably because OGR_BUILD_OPTIONAL_DRIVERS=OFF was explicitly set. You may conditionally enable the missing required driver by setting ${_dep}=ON")
                    elseif ( "${_dep}" MATCHES "GDAL_ENABLE_DRIVER_" AND NOT ${GDAL_BUILD_OPTIONAL_DRIVERS})
                        set(ERROR_MSG_EXTRA_HINT ", presumably because GDAL_BUILD_OPTIONAL_DRIVERS=OFF was explicitly set. You may conditionally enable the missing required driver by setting ${_dep}=ON")
                    endif()
                    message(FATAL_ERROR "${variable} cannot be enabled because condition ${_dep} is not met${ERROR_MSG_EXTRA_HINT}. To ignore this error (but the driver will not be built), set the GDAL_IGNORE_FAILED_CONDITIONS variable")
                else()
                    message(WARNING "${variable} cannot be enabled because condition ${_dep} is not met.")
                endif()
            endif()
            unset(TO_BE_REMOVED CACHE)
        endforeach()
    endif()
endmacro()

# gdal_dependent_format(format desc depend) do followings:
# - add subdirectory 'format'
# - define option "GDAL_ENABLE_DRIVER_NAME" then set to default OFF/ON
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
    check_depend_condition(GDAL_ENABLE_DRIVER_${key} "${depends}")
    cmake_dependent_option(GDAL_ENABLE_DRIVER_${key} "Set ON to build ${desc} format" ${GDAL_BUILD_OPTIONAL_DRIVERS}
                           "${depends}" OFF)
    add_feature_info(gdal_${key} GDAL_ENABLE_DRIVER_${key} "${desc}")
    if ((GDAL_ENABLE_DRIVER_${key} AND NOT _GDF_SKIP_ADD_SUBDIRECTORY) OR GDAL_REGISTER_DRIVER_${key}_FOR_LATER_PLUGIN)
        add_subdirectory(${format})
    endif ()
endmacro()

macro(gdal_format format desc)
    string(TOUPPER ${format} key desc)
    set(GDAL_ENABLE_DRIVER_${key} ON CACHE BOOL "" FORCE)
    add_feature_info(gdal_${key} GDAL_ENABLE_DRIVER_${key} "${desc}")
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
    option(GDAL_ENABLE_DRIVER_${key} "Set ON to build ${desc} format" ${GDAL_BUILD_OPTIONAL_DRIVERS})
    add_feature_info(gdal_${key} GDAL_ENABLE_DRIVER_${key} "${desc}")
    if (GDAL_ENABLE_DRIVER_${key} OR GDAL_REGISTER_DRIVER_${key}_FOR_LATER_PLUGIN)
        add_subdirectory(${format})
    endif ()
endmacro()

# ogr_dependent_driver(NAME desc depend) do followings:
# - define option "OGR_ENABLE_DRIVER_<name>" with default OFF
# - add subdirectory 'name'
# - when dependency specified by depend fails, force OFF

macro(ogr_dependent_driver name desc depend)
    string(TOUPPER ${name} key)
    check_depend_condition(OGR_ENABLE_DRIVER_${key} "${depend}")
    if( NOT("${key}" STREQUAL "GPKG" OR "${key}" STREQUAL "SQLITE" OR "${key}" STREQUAL "AVC") )
        cmake_dependent_option(OGR_ENABLE_DRIVER_${key} "Set ON to build OGR ${desc} driver" ${OGR_BUILD_OPTIONAL_DRIVERS}
                               "${depend}" OFF)
    endif()
    add_feature_info(ogr_${key} OGR_ENABLE_DRIVER_${key} "${desc}")
    if (OGR_ENABLE_DRIVER_${key} OR OGR_REGISTER_DRIVER_${key}_FOR_LATER_PLUGIN)
        add_subdirectory(${name})
    endif ()
endmacro()

# ogr_optional_driver(name desc) do followings:
# - define option "OGR_ENABLE_DRIVER_<name>" with default OFF
# - add subdirectory 'name' when enabled
macro(ogr_optional_driver name desc)
    string(TOUPPER ${name} key)
    option(OGR_ENABLE_DRIVER_${key} "Set ON to build OGR ${desc} driver" ${OGR_BUILD_OPTIONAL_DRIVERS})
    add_feature_info(ogr_${key} OGR_ENABLE_DRIVER_${key} "${desc}")
    if (OGR_ENABLE_DRIVER_${key} OR OGR_REGISTER_DRIVER_${key}_FOR_LATER_PLUGIN)
        add_subdirectory(${name})
    endif ()
endmacro()

# ogr_default_driver(name desc)
# - set "OGR_ENABLE_DRIVER_<name>" is ON but configurable.
# - add subdirectory "name"
macro(ogr_default_driver name desc)
    string(TOUPPER ${name} key)
    set(OGR_ENABLE_DRIVER_${key} ON CACHE BOOL "${desc}" FORCE)
    add_feature_info(ogr_${key} OGR_ENABLE_DRIVER_${key} "${desc}")
    add_subdirectory(${name})
endmacro()
macro(ogr_default_driver2 name key desc)
    set(OGR_ENABLE_DRIVER_${key} ON CACHE BOOL "${desc}" FORCE)
    add_feature_info(ogr_${key} OGR_ENABLE_DRIVER_${key} "${desc}")
    add_subdirectory(${name})
endmacro()

