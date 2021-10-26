# Select a preferred imported configuration from a target
function(select_imported_config target imported_conf)
    # We will first assign the value to a local variable _imported_conf, then assign
    # it to the function argument at the end.
    get_target_property(_imported_conf ${target} MAP_IMPORTED_CONFIG_${CMAKE_BUILD_TYPE})
    if (NOT _imported_conf)
        # Get available imported configurations by examining target properties
        get_target_property(_imported_conf ${target} IMPORTED_CONFIGURATIONS)
        # Find the imported configuration that we prefer.
        # We do this by making list of configurations in order of preference,
        # starting with ${CMAKE_BUILD_TYPE} and ending with the first imported_conf
        set(_preferred_confs ${CMAKE_BUILD_TYPE})
        list(GET _imported_conf 0 _fallback_conf)
        list(APPEND _preferred_confs RELWITHDEBINFO RELEASE DEBUG ${_fallback_conf})
        # Now find the first of these that is present in imported_conf
        cmake_policy(PUSH)
        cmake_policy(SET CMP0057 NEW) # support IN_LISTS
        foreach (_conf IN LISTS _preferred_confs)
            if (${_conf} IN_LIST _imported_conf)
               set(_imported_conf ${_conf})
               break()
            endif()
        endforeach()
        cmake_policy(POP)
    endif()
    # assign value to function argument
    set(${imported_conf} ${_imported_conf} PARENT_SCOPE)
endfunction()

