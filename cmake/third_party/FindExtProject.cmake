################################################################################
# Project:  external projects
# Purpose:  CMake build scripts
# Author:   Dmitry Baryshnikov, polimax@mail.ru
################################################################################
# Copyright (C) 2015-2019, NextGIS <info@nextgis.com>
# Copyright (C) 2015-2019 Dmitry Baryshnikov
#
# This script is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this script.  If not, see <http://www.gnu.org/licenses/>.
################################################################################

function(color_message text)

    string(ASCII 27 Esc)
    set(BoldGreen   "${Esc}[1;32m")
    set(ColourReset "${Esc}[m")

    message(STATUS "${BoldGreen}${text}${ColourReset}")

endfunction()

function(get_binary_package url repo repo_type repo_id exact_version is_static download_url name)
    include(third_party_util)
    get_compiler_version(COMPILER)
    get_prefix(STATIC_PREFIX ${is_static})

    if(repo_type STREQUAL "github") # TODO: Add gitlab here.
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/${repo}_latest.json)
            if(exact_version)
                file(DOWNLOAD
                    ${url}/repos/${repo}/releases/tags/v${exact_version}
                    ${CMAKE_BINARY_DIR}/${repo}_latest.json
                    TLS_VERIFY OFF
                )
            else()
                file(DOWNLOAD
                    ${url}/repos/${repo}/releases/latest
                    ${CMAKE_BINARY_DIR}/${repo}_latest.json
                    TLS_VERIFY OFF
                )
            endif()
        endif()
        # Get assets files.
        file(READ ${CMAKE_BINARY_DIR}/${repo}_latest.json _JSON_CONTENTS)

        include(JSONParser)
        sbeParseJson(api_request _JSON_CONTENTS)
        foreach(asset_id ${api_request.assets})
            if(exact_version)
                string(FIND ${api_request.assets_${asset_id}.browser_download_url} "${exact_version}-${STATIC_PREFIX}${COMPILER}.zip" IS_FOUND)
            else()
                string(FIND ${api_request.assets_${asset_id}.browser_download_url} "${STATIC_PREFIX}${COMPILER}.zip" IS_FOUND) 
                # In this case we get static and shared. Add one more check.
                if(NOT is_static)
                    string(FIND ${api_request.assets_${asset_id}.browser_download_url} "static-${COMPILER}.zip" IS_FOUND_STATIC)
                    if(IS_FOUND_STATIC GREATER 0)
                        continue()
                    endif()
                endif()

                if(NOT ANDROID)
                    string(FIND ${api_request.files_${asset_id}.name} "android-" IS_FOUND_OS)
                    if(IS_FOUND_OS GREATER 0)
                        continue()
                    endif()
                endif()
            
                if(IOS)
                    string(FIND ${api_request.files_${asset_id}.name} "ios-" IS_FOUND_OS)
                    if(IS_FOUND_OS GREATER 0)
                        continue()
                    endif()
                endif()
            endif()
            if(IS_FOUND GREATER 0)
                color_message("Found binary package ${api_request.assets_${asset_id}.browser_download_url}")
                set(${download_url} ${api_request.assets_${asset_id}.browser_download_url} PARENT_SCOPE)
                string(REPLACE ".zip" "" FOLDER_NAME ${api_request.assets_${asset_id}.name} )
                set(${name} ${FOLDER_NAME} PARENT_SCOPE)
                break()
            endif()
        endforeach()

        sbeClearJson(api_request)
    elseif(repo_type STREQUAL "repka")
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/${repo}_latest.json)
            if(exact_version)
                file(DOWNLOAD
                    ${url}/api/repo/${repo_id}/borsch?packet_name=${repo}&release_tag=${exact_version}
                    ${CMAKE_BINARY_DIR}/${repo}_latest.json
                    TLS_VERIFY OFF
                )
            else()
                file(DOWNLOAD
                    ${url}/api/repo/${repo_id}/borsch?packet_name=${repo}&release_tag=latest
                    ${CMAKE_BINARY_DIR}/${repo}_latest.json
                    TLS_VERIFY OFF
                )
            endif()
        endif()
        # Get assets files.
        file(READ ${CMAKE_BINARY_DIR}/${repo}_latest.json _JSON_CONTENTS)

        include(JSONParser)
        sbeParseJson(api_request _JSON_CONTENTS)
        foreach(asset_id ${api_request.files})
            string(FIND ${api_request.files_${asset_id}.name} "${STATIC_PREFIX}${COMPILER}.zip" IS_FOUND)
            # In this case we get static and shared. Add one more check.
            if(NOT ANDROID)
                string(FIND ${api_request.files_${asset_id}.name} "android-" IS_FOUND_OS)
                if(IS_FOUND_OS GREATER 0)
                    continue()
                endif()
            endif()
        
            if(IOS)
                string(FIND ${api_request.files_${asset_id}.name} "ios-" IS_FOUND_OS)
                if(IS_FOUND_OS GREATER 0)
                    continue()
                endif()
            endif()
        
            if(NOT is_static)
                string(FIND ${api_request.files_${asset_id}.name} "static-" IS_FOUND_STATIC)
                if(IS_FOUND_STATIC GREATER 0)
                    continue()
                endif()
            endif()

            if(IS_FOUND GREATER 0)
                color_message("Found binary package ${api_request.files_${asset_id}.name}")
                set(${download_url} ${url}/api/asset/${api_request.files_${asset_id}.id}/download PARENT_SCOPE)
                string(REPLACE ".zip" "" FOLDER_NAME ${api_request.files_${asset_id}.name} )
                set(${name} ${FOLDER_NAME} PARENT_SCOPE)
                break()
            endif()
        endforeach()
    endif()

endfunction()

function(find_extproject name)
    set(options OPTIONAL EXACT)
    set(oneValueArgs VERSION SHARED)
    set(multiValueArgs CMAKE_ARGS COMPONENTS NAMES)
    cmake_parse_arguments(find_extproject "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set default third party lib path.
    if(NOT DEFINED EP_PREFIX)
        set(EP_PREFIX "${CMAKE_BINARY_DIR}/third-party")
    endif()

    # Set default third party install path.
    if(NOT DEFINED EXT_INSTALL_DIR)
        set(EXT_INSTALL_DIR "${EP_PREFIX}/install")
    endif()

    # Get some properties from <cmakemodules>/FindExt${name}.cmake file.
    include(FindExt${name})

    if(find_extproject_EXACT)
        set(TEST_VERSION ${find_extproject_VERSION})
    else()
        set(TEST_VERSION IGNORE)
    endif()

    if(NOT DEFINED repo_bin)
        set(repo_bin ${repo})
    endif()
    if(NOT DEFINED repo_bin_type)
        set(repo_bin_type ${repo_type})
    endif()
    if(NOT DEFINED repo_bin_id)
        set(repo_bin_id 0)
    endif()

    if(NOT DEFINED find_extproject_SHARED AND (BUILD_SHARED_LIBS OR OSX_FRAMEWORK))
        set(IS_STATIC NO)
    elseif(find_extproject_SHARED)
        set(IS_STATIC NO)
    else()
        set(IS_STATIC YES)
    endif()

    get_binary_package(${repo_bin_url} ${repo_bin} ${repo_bin_type} ${repo_bin_id} ${TEST_VERSION} ${IS_STATIC} BINARY_URL BINARY_NAME)

    if(BINARY_URL)
        # Download binary build files.
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/${name}.zip)
            file(DOWNLOAD
                ${BINARY_URL}
                ${CMAKE_BINARY_DIR}/${name}.zip
                TLS_VERIFY OFF
            )
        endif()

        # Extact files.
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_INSTALL_DIR}
        )
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xfz ${CMAKE_BINARY_DIR}/${name}.zip
            WORKING_DIRECTORY ${EXT_INSTALL_DIR}
        )
        # Execute find_package and send version, libraries, includes upper cmake script.
        # The CMake folder in root folder is prefered
        string(TOUPPER ${name} UPPER_NAME)
        string(TOLOWER ${name} LOWER_NAME)
        if(CMAKE_CROSSCOMPILING)
            if(find_extproject_NAMES)
                foreach(PNAME ${find_extproject_NAMES})
                    if(EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${PNAME}/CMake)
                        set(${name}_DIR ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${PNAME}/CMake)
                        break()
                    endif()
                endforeach()       
            else()
                if(EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${name}/CMake)
                    set(${name}_DIR ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${name}/CMake)
                elseif(EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${UPPER_NAME}/CMake)
                    set(${name}_DIR ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${UPPER_NAME}/CMake)
                elseif(EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${LOWER_NAME}/CMake)
                    set(${name}_DIR ${EXT_INSTALL_DIR}/${BINARY_NAME}/share/${LOWER_NAME}/CMake)
                endif()
            endif()     
        elseif(OSX_FRAMEWORK AND NOT EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/CMake AND EXISTS ${EXT_INSTALL_DIR}/${BINARY_NAME}/Library/Frameworks)
            set(CMAKE_PREFIX_PATH ${EXT_INSTALL_DIR}/${BINARY_NAME}/Library/Frameworks)
        else()
            set(CMAKE_PREFIX_PATH ${EXT_INSTALL_DIR}/${BINARY_NAME})
        endif()

        if(find_extproject_COMPONENTS)
            set(FIND_PROJECT_ARG ${FIND_PROJECT_ARG} COMPONENTS ${find_extproject_COMPONENTS})
        endif()

        if(find_extproject_NAMES)
            set(FIND_PROJECT_ARG ${FIND_PROJECT_ARG} NAMES ${find_extproject_NAMES})
        endif()

        find_package(${name} NO_MODULE ${FIND_PROJECT_ARG})
        
        set(${UPPER_NAME}_FOUND ${${UPPER_NAME}_FOUND} PARENT_SCOPE)
        set(${UPPER_NAME}_VERSION ${${UPPER_NAME}_VERSION} PARENT_SCOPE)
        set(${UPPER_NAME}_VERSION_STR ${${UPPER_NAME}_VERSION_STR} PARENT_SCOPE)
        set(${UPPER_NAME}_LIBRARIES ${${UPPER_NAME}_LIBRARIES} PARENT_SCOPE)
        set(${UPPER_NAME}_INCLUDE_DIRS ${${UPPER_NAME}_INCLUDE_DIRS} PARENT_SCOPE)

        foreach(TARGETG ${${UPPER_NAME}_LIBRARIES})
            if(TARGET ${TARGETG})
                set_target_properties(${TARGETG} PROPERTIES IMPORTED_GLOBAL TRUE)
            endif()
        endforeach()
        return()
    endif()

    # Set default third party build path
    if(NOT DEFINED EXT_BUILD_DIR)
        set(EXT_BUILD_DIR "${EP_PREFIX}/build")
    endif()

    # Set default third party tmp path
    if(NOT DEFINED EXT_TMP_DIR)
        set(EXT_TMP_DIR "${EXT_BUILD_DIR}/tmp")
    endif()

    # Set default third party download path
    if(NOT DEFINED EXT_DOWNLOAD_DIR)
        set(EXT_DOWNLOAD_DIR "${EP_PREFIX}/src")
    endif()

    if(NOT DEFINED SUPPRESS_VERBOSE_OUTPUT)
        set(SUPPRESS_VERBOSE_OUTPUT TRUE)
    endif()

    # Prepare options from current project.
    list(APPEND find_extproject_CMAKE_ARGS -DSKIP_DEFAULTS=ON)
    list(APPEND find_extproject_CMAKE_ARGS -DEP_PREFIX=${EP_PREFIX})
    list(APPEND find_extproject_CMAKE_ARGS -DEXT_BUILD_DIR=${EXT_BUILD_DIR})
    list(APPEND find_extproject_CMAKE_ARGS -DEXT_TMP_DIR=${EXT_TMP_DIR})
    list(APPEND find_extproject_CMAKE_ARGS -DEXT_DOWNLOAD_DIR=${EXT_DOWNLOAD_DIR})
    list(APPEND find_extproject_CMAKE_ARGS -DEXT_INSTALL_DIR=${EXT_INSTALL_DIR})
    list(APPEND find_extproject_CMAKE_ARGS -DSUPPRESS_VERBOSE_OUTPUT=${SUPPRESS_VERBOSE_OUTPUT})
    list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH})
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    endif()

    if(ANDROID)
        list(APPEND find_extproject_CMAKE_ARGS -DANDROID=ON)

        # Configurable variables from android-sdk/ndk-bundle/build/cmake/android.toolchain.cmake (Android NDK 14.0.3770861)
        # Modeled after the ndk-build system.
        # For any variables defined in:
        #         https://developer.android.com/ndk/guides/android_mk.html
        #         https://developer.android.com/ndk/guides/application_mk.html
        # if it makes sense for CMake, then replace LOCAL, APP, or NDK with ANDROID, and
        # we have that variable below.
        # The exception is ANDROID_TOOLCHAIN vs NDK_TOOLCHAIN_VERSION.
        # Since we only have one version of each gcc and clang, specifying a version
        # doesn't make much sense.
        if(DEFINED ANDROID_NDK)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_NDK=${ANDROID_NDK})
        endif()
        if(DEFINED ANDROID_TOOLCHAIN)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_TOOLCHAIN=${ANDROID_TOOLCHAIN})
        endif()
        if(DEFINED ANDROID_ABI)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI})
        endif()
        if(DEFINED ANDROID_PLATFORM)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_PLATFORM=${ANDROID_PLATFORM})
        endif()
        if(DEFINED ANDROID_STL)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_STL=${ANDROID_STL})
        endif()
        if(DEFINED ANDROID_PIE)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_PIE=${ANDROID_PIE})
        endif()
        if(DEFINED ANDROID_CPP_FEATURES)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_CPP_FEATURES=${ANDROID_CPP_FEATURES})
        endif()
        if(DEFINED ANDROID_ALLOW_UNDEFINED_SYMBOLS)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_ALLOW_UNDEFINED_SYMBOLS=${ANDROID_ALLOW_UNDEFINED_SYMBOLS})
        endif()
        if(DEFINED ANDROID_ARM_MODE)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_ARM_MODE=${ANDROID_ARM_MODE})
        endif()
        if(DEFINED ANDROID_ARM_NEON)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_ARM_NEON=${ANDROID_ARM_NEON})
            set(ENABLE_NEON ${ANDROID_ARM_NEON})
        endif()
        if(DEFINED ANDROID_DISABLE_NO_EXECUTE)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_DISABLE_NO_EXECUTE=${ANDROID_DISABLE_NO_EXECUTE})
        endif()
        if(DEFINED ANDROID_DISABLE_RELRO)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_DISABLE_RELRO=${ANDROID_DISABLE_RELRO})
        endif()
        if(DEFINED ANDROID_DISABLE_FORMAT_STRING_CHECKS)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_DISABLE_FORMAT_STRING_CHECKS=${ANDROID_DISABLE_FORMAT_STRING_CHECKS})
        endif()
        if(DEFINED ANDROID_CCACHE)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_CCACHE=${ANDROID_CCACHE})
        endif()
        if(DEFINED ANDROID_UNIFIED_HEADERS)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_UNIFIED_HEADERS=${ANDROID_UNIFIED_HEADERS})
        endif()

        if(DEFINED ANDROID_SYSROOT_ABI)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_SYSROOT_ABI=${ANDROID_SYSROOT_ABI}) # arch
        endif()
        if(DEFINED ANDROID_PLATFORM_LEVEL)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_PLATFORM_LEVEL=${ANDROID_PLATFORM_LEVEL})
        endif()

        # Variables are only for compatibility.
        if(DEFINED ANDROID_NATIVE_API_LEVEL)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL})
        endif()
        if(DEFINED ANDROID_TOOLCHAIN_NAME)
            list(APPEND find_extproject_CMAKE_ARGS -DANDROID_TOOLCHAIN_NAME=${ANDROID_TOOLCHAIN_NAME})
        endif()
    endif()

    if(IOS)
        list(APPEND find_extproject_CMAKE_ARGS -DIOS=ON)
        if(DEFINED CMAKE_OSX_ARCHITECTURES)
            list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES})
        endif()
        if(DEFINED IOS_PLATFORM)
            list(APPEND find_extproject_CMAKE_ARGS -DIOS_PLATFORM=${IOS_PLATFORM})
        endif()
        if(DEFINED IOS_ARCH)
            list(APPEND find_extproject_CMAKE_ARGS -DIOS_ARCH=${IOS_ARCH})
        endif()
        if(DEFINED ENABLE_BITCODE)
            list(APPEND find_extproject_CMAKE_ARGS -DENABLE_BITCODE=${ENABLE_BITCODE})
        endif()
    endif()

    if(DEFINED ENABLE_NEON)
        list(APPEND find_extproject_CMAKE_ARGS -DENABLE_NEON=${ENABLE_NEON})
    endif()

    if(DEFINED CMAKE_OSX_SYSROOT)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT})
    endif()
    if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()

    set_property(DIRECTORY PROPERTY "EP_PREFIX" ${EP_PREFIX})

    # search CMAKE_INSTALL_PREFIX
    string (REGEX MATCHALL "(^|;)-DCMAKE_INSTALL_PREFIX=[A-Za-z0-9_]*" _matchedVars "${find_extproject_CMAKE_ARGS}")
    list(LENGTH _matchedVars _list_size)
    if(_list_size EQUAL 0)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXT_INSTALL_DIR})
    endif()
    unset(_matchedVars)

    if(OSX_FRAMEWORK)
        list(APPEND find_extproject_CMAKE_ARGS -DOSX_FRAMEWORK=${OSX_FRAMEWORK})
        list(APPEND find_extproject_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
        list(APPEND find_extproject_CMAKE_ARGS -DBUILD_STATIC_LIBS=OFF)
    else()
        # search BUILD_SHARED_LIBS
        if(NOT DEFINED find_extproject_SHARED)
            list(APPEND find_extproject_CMAKE_ARGS -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS})
            set(find_extproject_SHARED ${BUILD_SHARED_LIBS})
        elseif(find_extproject_SHARED)
            list(APPEND find_extproject_CMAKE_ARGS -DBUILD_SHARED_LIBS=ON)
        else()
            list(APPEND find_extproject_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
            list(APPEND find_extproject_CMAKE_ARGS -DBUILD_STATIC_LIBS=ON)
        endif()
    endif()

    # set some arguments
    if(CMAKE_GENERATOR)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_GENERATOR=${CMAKE_GENERATOR})
    endif()
    if(CMAKE_MAKE_PROGRAM)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
    endif()
    if(CMAKE_BUILD_TYPE)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
    endif()
    # list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES})
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND find_extproject_CMAKE_ARGS -DCMAKE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET})
    endif()

    get_cmake_property(_variableNames VARIABLES)
    string (REGEX MATCHALL "(^|;)WITH_[A-Za-z0-9_]*" _matchedVars "${_variableNames}")
    foreach(_variableName ${_matchedVars})
        if(NOT SUPPRESS_VERBOSE_OUTPUT)
            message(STATUS "${_variableName}=${${_variableName}}")
        endif()
        list(APPEND find_extproject_CMAKE_ARGS -D${_variableName}=${${_variableName}})
    endforeach()

    # For crossplatform build search packages in host system
    if(CMAKE_CROSSCOMPILING OR ANDROID OR IOS)
        set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
        set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER )
        set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER )
    endif()

    find_package(Git)
    if(NOT GIT_FOUND)
      message(FATAL_ERROR "git is required")
      return()
    endif()

    if(find_extproject_EXACT) # If version mark exact, set branch name as tags/v1.X.X
        set(repo_branch "tags/v${find_extproject_VERSION}")
    elseif(NOT DEFINED repo_branch) # If repo_branch is not defined - set it to master.
        set(repo_branch master)
    endif()

    if(repo_type STREQUAL "github")
        set(repo_url https://gitee.com/${repo})
    endif()

    # Clone and config repository before ExternalProject
    set(EXT_SOURCE_DIR "${EXT_DOWNLOAD_DIR}/${name}_EP")
    set(EXT_STAMP_DIR "${EXT_BUILD_DIR}/${name}_EP-stamp")
    set(EXT_BINARY_DIR "${EXT_BUILD_DIR}/${name}_EP-build")

    # Add external project target
    if(NOT DEFINED EXT_UPDATE_DISCONNECTED)
        set(EXT_UPDATE_DISCONNECTED FALSE)
    endif()

    include(ExternalProject)
    ExternalProject_Add(${name}_EP
        TMP_DIR ${EXT_TMP_DIR}
        STAMP_DIR ${EXT_STAMP_DIR}
        DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
        SOURCE_DIR ${EXT_SOURCE_DIR}
        BINARY_DIR ${EXT_BINARY_DIR}
        INSTALL_DIR ${EXT_INSTALL_DIR}
        GIT_REPOSITORY ${repo_url}
        GIT_TAG ${repo_branch}
        GIT_SHALLOW TRUE
        CMAKE_ARGS ${find_extproject_CMAKE_ARGS}
        UPDATE_DISCONNECTED ${EXT_UPDATE_DISCONNECTED}
        DOWNLOAD_COMMAND "" # Git clone is executed below
    )

    if(NOT EXISTS "${EXT_SOURCE_DIR}/.git")
        color_message("Git clone ${repo_name} ...")

        set(error_code 1)
        set(number_of_tries 0)
        while(error_code AND number_of_tries LESS 3)
            set(BRANCH)
            if(find_extproject_EXACT)
                set(BRANCH --branch ${repo_branch})
            endif()
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone ${BRANCH} --depth 1 ${repo_url} ${name}_EP
                WORKING_DIRECTORY  ${EXT_DOWNLOAD_DIR}
                RESULT_VARIABLE error_code
            )
            math(EXPR number_of_tries "${number_of_tries} + 1")
        endwhile()

        if(error_code)
            message(FATAL_ERROR "Failed to clone repository ${repo_url}")
            return()
        endif()

        color_message("Configure ${repo_name} ...")
        execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_SOURCE_DIR}
            ${find_extproject_CMAKE_ARGS}
            WORKING_DIRECTORY ${EXT_BINARY_DIR})
    endif()

    if(CMAKE_CROSSCOMPILING OR ANDROID OR IOS)
        set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
        set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
        set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
    endif()

    string(TOUPPER ${name} UPPER_NAME)
    if(EXISTS ${EXT_BINARY_DIR}/${UPPER_NAME}Config.cmake)
        include(${EXT_BINARY_DIR}/${UPPER_NAME}Config.cmake)
    else()
        foreach(ALT_NAME ${find_extproject_NAMES})
            string(TOUPPER ${ALT_NAME} ALT_UPPER_NAME)
            if(EXISTS ${EXT_BINARY_DIR}/${ALT_UPPER_NAME}Config.cmake)
                include(${EXT_BINARY_DIR}/${ALT_UPPER_NAME}Config.cmake)
                break()
            endif()
        endforeach()
    endif()

    set(${UPPER_NAME}_FOUND ${${UPPER_NAME}_FOUND} PARENT_SCOPE)
    set(${UPPER_NAME}_VERSION ${${UPPER_NAME}_VERSION} PARENT_SCOPE)
    set(${UPPER_NAME}_VERSION_STR ${${UPPER_NAME}_VERSION_STR} PARENT_SCOPE)
    set(${UPPER_NAME}_LIBRARIES ${${UPPER_NAME}_LIBRARIES} PARENT_SCOPE)
    set(${UPPER_NAME}_INCLUDE_DIRS ${${UPPER_NAME}_INCLUDE_DIRS} PARENT_SCOPE)

    set_target_properties(${${UPPER_NAME}_LIBRARIES} PROPERTIES IMPORTED_GLOBAL TRUE)

    add_dependencies(${${UPPER_NAME}_LIBRARIES} ${name}_EP)

    # On static build we need all targets in TARGET_LINK_LIB
    if(ALT_UPPER_NAME)
        set(EXPORTS_PATHS ${EXPORTS_PATHS} ${EXT_BINARY_DIR}/${ALT_UPPER_NAME}Targets.cmake PARENT_SCOPE)
    else()
        set(EXPORTS_PATHS ${EXPORTS_PATHS} ${EXT_BINARY_DIR}/${UPPER_NAME}Targets.cmake PARENT_SCOPE)
    endif()

    # For static builds we need all libraries list in main project.
    if(EXISTS ${EXT_BINARY_DIR}/ext_options.cmake)
        include(${EXT_BINARY_DIR}/ext_options.cmake)

        foreach(INCLUDE_EXPORT_PATH ${INCLUDE_EXPORTS_PATHS})
            include(${INCLUDE_EXPORT_PATH})
        endforeach()

        # Add include into ext_options.cmake.
        set(WITHOPT "${WITHOPT}include(${EXT_BINARY_DIR}/ext_options.cmake)\n" PARENT_SCOPE)
    endif()

endfunction()
