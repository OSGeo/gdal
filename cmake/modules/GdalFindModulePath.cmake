list(INSERT CMAKE_MODULE_PATH 0
    "${CMAKE_CURRENT_LIST_DIR}/packages"
    "${CMAKE_CURRENT_LIST_DIR}/thirdparty")

# Backported modules from cmake versions

set(GDAL_VENDORED_FIND_MODULES_CMAKE_VERSIONS 3.20 3.16 3.14 3.13 3.12)

foreach(_version IN LISTS GDAL_VENDORED_FIND_MODULES_CMAKE_VERSIONS)
    if(CMAKE_VERSION VERSION_LESS "${_version}")
        list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/${_version}")
    endif()
endforeach()
