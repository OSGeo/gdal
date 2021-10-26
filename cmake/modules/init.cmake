set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}
    # find packages
    ${CMAKE_CURRENT_LIST_DIR}/packages
    # thirdparty modules
    ${CMAKE_CURRENT_LIST_DIR}/thirdparty
    # GDAL specific helpers
    ${CMAKE_CURRENT_LIST_DIR}/../helpers
    ${CMAKE_MODULE_PATH})

# Backported modules from cmake versions
if(CMAKE_VERSION VERSION_LESS 3.16)
    set(CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_LIST_DIR}/3.16
        ${CMAKE_MODULE_PATH})
endif()
if(CMAKE_VERSION VERSION_LESS 3.14)
    set(CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_LIST_DIR}/3.14
        ${CMAKE_MODULE_PATH})
endif()
if(CMAKE_VERSION VERSION_LESS 3.13)
    set(CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_LIST_DIR}/3.13
        ${CMAKE_MODULE_PATH})
endif()
if(CMAKE_VERSION VERSION_LESS 3.12)
    set(CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_LIST_DIR}/3.12
        ${CMAKE_MODULE_PATH})
endif()
