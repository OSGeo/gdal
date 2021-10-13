#
# SystemSummary.cmake - show summary
#
# Copyright (c) 2017,2018 Hiroshi Miura
#
# Original copyright for macros:
#############################################
#
# ucm.cmake - useful cmake macros
#
# Copyright (c) 2016 Viktor Kirilov
#
# Distributed under the MIT Software License
# See accompanying file LICENSE.txt or copy at
# https://opensource.org/licenses/MIT
#
# The documentation can be found at the library's page:
# https://github.com/onqtam/ucm

# Gathers all lists of flags
macro(gather_flags with_linker result)
    set(${result} "")
    # add the main flags without a config
    list(APPEND ${result} CMAKE_C_FLAGS)
    list(APPEND ${result} CMAKE_CXX_FLAGS)
    list(APPEND ${result} CMAKE_CXX11_STANDARD_COMPILE_OPTION)
    list(APPEND ${result} CMAKE_CXX11_EXTENSION_COMPILE_OPTION)

    if(${with_linker})
        list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS)
    endif()

    if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        # handle single config generators - like makefiles/ninja - when CMAKE_BUILD_TYPE is set
        string(TOUPPER ${CMAKE_BUILD_TYPE} config)
        list(APPEND ${result} CMAKE_C_FLAGS_${config})
        list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
        if(${with_linker})
            list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
        endif()
    else()
        # handle multi config generators (like msvc, xcode)
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${config} config)
            list(APPEND ${result} CMAKE_C_FLAGS_${config})
            list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
            if(${with_linker})
                list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
            endif()
        endforeach()
    endif()
endmacro()

# print_flags
# Prints all compiler flags for all configurations
macro(print_compiler_flags)
    set(WITH_LINKER ON)
    gather_flags(${WITH_LINKER} allflags)
    message(STATUS "")
    foreach(flags ${allflags})
        message(STATUS "  ${flags}:              ${${flags}}")
    endforeach()
    message(STATUS "")
endmacro()

function(system_summary)
    set(_options)
    set(_oneValueArgs DESCRIPTION)
    set(_multiValueArgs)
    cmake_parse_arguments(_SUMMARY "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if(DESCRIPTION)
        message(STATUS "${_SUMMARY_DESCRIPTION}")
    endif()
    message(STATUS "  Target system:             ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  Installation directory:    ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "  C++ Compiler type:         ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "  C compile command line:    ${CMAKE_C_COMPILER_LAUNCHER} ${CMAKE_C_COMPILER}")
    message(STATUS "  C++ compile command line:  ${CMAKE_CXX_COMPILER_LAUNCHER} ${CMAKE_CXX_COMPILER}")
    print_compiler_flags()
endfunction()
