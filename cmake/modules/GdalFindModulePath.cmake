list(INSERT CMAKE_MODULE_PATH 0
    "${CMAKE_CURRENT_LIST_DIR}/packages"
    "${CMAKE_CURRENT_LIST_DIR}/thirdparty")

# Backported modules from cmake versions
foreach(_version IN ITEMS 3.20 3.16 3.14 3.13 3.12)
    if(CMAKE_VERSION VERSION_LESS "${_version}")
        list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/${_version}")
    endif()
endforeach()
