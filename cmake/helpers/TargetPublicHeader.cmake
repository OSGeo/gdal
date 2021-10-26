# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
TargetPublicHeader
------------------


#]=======================================================================]

function(_convert_to_full_path _result_var _files_var)
    set(${_result_var})
    foreach(f IN LISTS ${_files_var})
        if(IS_ABSOLUTE ${f} OR
           f MATCHES "^\$") # expressions
            list(APPEND ${_result_var} ${f})
        else()
            get_filename_component(r ${f} ABSOLUTE)
            list(APPEND ${_result_var} ${r})
        endif()
    endforeach()
    set(${_result_var} ${${_result_var}} PARENT_SCOPE)
endfunction()

function(target_public_header)
    set(_options BUILTIN)
    set(_oneValueArgs TARGET)
    set(_multiValueArgs HEADERS)
    cmake_parse_arguments(_PUBLIC "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
    if(NOT _PUBLIC_HEADERS)
        message(FATAL_ERROR "add_public_header: HEADERS is mandatory argument")
    endif()
    _convert_to_full_path(FILE_LISTS _PUBLIC_HEADERS)
    set_property(TARGET ${GDAL_LIB_TARGET_NAME} APPEND PROPERTY PUBLIC_HEADER "${FILE_LISTS}")
endfunction()
