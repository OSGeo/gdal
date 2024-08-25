# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

find_package(Doxygen)
find_program(SPHINX_BUILD sphinx-build)
find_program(MAKE_EXECUTABLE make)

if (UNIX
    AND (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    AND DOXYGEN_FOUND
    AND SPHINX_BUILD
    AND MAKE_EXECUTABLE)
    set(BUILD_DOCS_DEFAULT ON)
else()
    set(BUILD_DOCS_DEFAULT OFF)
endif()
option(BUILD_DOCS "Set to ON to define documentation targets: 'html', 'latexpdf', 'man', 'doxygen', 'doxygen_check_warnings', 'spelling', 'clean_doc'" ${BUILD_DOCS_DEFAULT})

if (BUILD_DOCS)
    if (NOT UNIX)
        message(FATAL_ERROR "BUILD_DOCS=ON requires a UNIX environment")
    endif()
    if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
        message(FATAL_ERROR "BUILD_DOCS=ON not compatible of in-source builds (CMAKE_SOURCE_DIR=CMAKE_BINARY_DIR)")
    endif()
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "BUILD_DOCS=ON requires Doxygen")
    endif()
    if (NOT SPHINX_BUILD)
        message(FATAL_ERROR "BUILD_DOCS=ON requires sphinx-build")
    endif()
    if (NOT MAKE_EXECUTABLE)
        message(FATAL_ERROR "BUILD_DOCS=ON requires 'make' executable")
    endif()

    set(DOC_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/build")

    add_custom_target(
        doxygen
        COMMAND ${MAKE_EXECUTABLE} doxygen BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        doxygen_check_warnings
        COMMAND ${MAKE_EXECUTABLE} doxygen_check_warnings BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        html
        COMMAND ${MAKE_EXECUTABLE} html BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        latexpdf
        COMMAND ${MAKE_EXECUTABLE} latexpdf BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        man
        COMMAND ${MAKE_EXECUTABLE} man BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        spelling
        COMMAND ${MAKE_EXECUTABLE} spelling BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(
        clean_doc
        COMMAND ${MAKE_EXECUTABLE} clean BUILDDIR=${DOC_BUILDDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    set_property(
        TARGET clean_doc
        APPEND
        PROPERTY ADDITIONAL_CLEAN_FILES ${DOC_BUILDDIR})
endif ()
