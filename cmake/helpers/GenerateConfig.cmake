
include(SelectImportedConfig)
include(SplitLibraryToCFlags)

function(get_dep_libs _target _link _output_var)
    set(_DEP_LIBS "")
    get_property(_LIBS GLOBAL PROPERTY ${_link})
    list(REMOVE_DUPLICATES _LIBS)
    foreach(_lib IN LISTS _LIBS)
        if(TARGET ${_lib})
            # will be IMPORTED TARGET
            get_property(_type TARGET ${_lib} PROPERTY TYPE)
            if(_type STREQUAL "INTERFACE_LIBRARY")
                # IMPORTED INTERFACE TARGET
                # We are only able to access INTERFACE_* property
                get_property(_res TARGET ${_lib} PROPERTY INTERFACE_LINK_LIBRARIES)
                if(_res)
                    split_library_to_cflags("${_imp}" _res)
                    list(APPEND _DEP_LIBS "${_res}")
                endif()
            elseif(_type STREQUAL "UNKNOWN_LIBRARY")
                # IMPORTED UNKOWN
                get_property(_res TARGET ${_lib} PROPERTY IMPORTED_CONFIGURATIONS SET)
                if(_res) # use imported target with configurations
                    select_imported_config(${_target} _conf)
                    if(NOT _conf)
                        set(_conf RELEASE)
                    endif()
                    string(TOUPPER ${_conf} _BT)
                    get_property(_imp TARGET ${_lib} PROPERTY IMPORTED_LOCATION_${_BT})
                    split_library_to_cflags("${_imp}" _res)
                    list(APPEND _DEP_LIBS "${_res}")
                else() # just use default location
                    get_property(_imp TARGET ${_lib} PROPERTY IMPORTED_LOCATION)
                    split_library_to_cflags("${_imp}" _val)
                    list(APPEND _DEP_LIBS "${_val}")
                endif()
            endif()
        else()
            split_library_to_cflags("${_lib}" _res)
            if(_res)
                list(APPEND _DEP_LIBS "${_res}")
            endif()
        endif()
    endforeach()
    string(REPLACE ";" " " _DEP_LIBS "${_DEP_LIBS}")
    set(${_output_var} "${_DEP_LIBS}" PARENT_SCOPE)
endfunction()

function(generate_config _target _link _template _output)
    if(NOT DEFINED CMAKE_INSTALL_PREFIX)
        set(CONFIG_PREFIX "/usr/local") # default
    else()
        set(CONFIG_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif()
    set(CONFIG_CFLAGS "-I${CONFIG_PREFIX}/include")
    get_property(_target_lib_name TARGET ${_target} PROPERTY OUTPUT_NAME)

    set(CONFIG_DATA "${CONFIG_PREFIX}/share/${_target_lib_name}")
    if(CONFIG_PREFIX STREQUAL "/usr")
        set(CONFIG_LIBS "${CMAKE_LINK_LIBRARY_FLAG}${_target_lib_name}")
    else()
        set(CONFIG_LIBS "${CMAKE_LIBRARY_PATH_FLAG}${CONFIG_PREFIX}/lib ${CMAKE_LINK_LIBRARY_FLAG}${_target_lib_name}")
    endif()

    # dep-libs
    get_dep_libs(${_target} ${_link} CONFIG_DEP_LIBS)

    get_filename_component(_output_dir ${_output} DIRECTORY)
    get_filename_component(_output_name ${_output} NAME)
    configure_file(${_template} ${_output_dir}/tmp/${_output_name} @ONLY)
    file(COPY ${_output_dir}/tmp/${_output_name}
         DESTINATION ${_output_dir}
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

endfunction()
