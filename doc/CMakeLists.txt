# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

find_package(Doxygen)
find_program(SPHINX_BUILD sphinx-build)


if ((NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    AND DOXYGEN_FOUND
    AND SPHINX_BUILD)
    set(BUILD_DOCS_DEFAULT ON)
else()
    set(BUILD_DOCS_DEFAULT OFF)
endif()

option(BUILD_DOCS "Set to ON to define documentation targets: 'html', 'latexpdf', 'man', 'doxygen_xml', 'doxygen_html' " ${BUILD_DOCS_DEFAULT})

option(DOXYGEN_FAIL_ON_WARNINGS "Set to ON to fail the build if Doxygen produces warnings" OFF)

if (BUILD_DOCS)
    if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
        message(FATAL_ERROR "BUILD_DOCS=ON not compatible of in-source builds (CMAKE_SOURCE_DIR=CMAKE_BINARY_DIR)")
    endif()
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "BUILD_DOCS=ON requires Doxygen")
    endif()
    if (NOT SPHINX_BUILD)
        message(FATAL_ERROR "BUILD_DOCS=ON requires sphinx-build")
    endif()

    ####################################################################################################
    # Sphinx configuration
    ####################################################################################################

    # Determine environment variables so that Sphinx can load the gdal Python module without installing it.
    include(GdalSetRuntimeEnv)
    gdal_set_runtime_env(BUILD_RUN_ENV)

    set(SPHINX_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)
    set(SPHINX_BUILD_OPTS "--jobs=auto" "--show-traceback")
    file(MAKE_DIRECTORY ${SPHINX_BUILD_DIR})
    file(MAKE_DIRECTORY ${SPHINX_BUILD_DIR}/html_extra)

    file(GLOB_RECURSE SPHINX_SOURCE_FILES CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/source/*.rst
            ${CMAKE_CURRENT_SOURCE_DIR}/source/*.py)

    ####################################################################################################
    # Doxygen XML and HTML outputs
    ####################################################################################################

    # Create a dependency between source files and Doxygen
    file(GLOB_RECURSE DOXYGEN_SOURCE_FILES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/alg/*.cpp
            ${CMAKE_SOURCE_DIR}/app/*.cpp
            ${CMAKE_SOURCE_DIR}/frmts/gdalallregister.cpp
            ${CMAKE_SOURCE_DIR}/frmts/vrt/*.cpp
            ${CMAKE_SOURCE_DIR}/gcore/*.cpp
            ${CMAKE_SOURCE_DIR}/ogr/*.cpp
            ${CMAKE_SOURCE_DIR}/port/*.cpp
            ${CMAKE_SOURCE_DIR}/gnm/*.cpp
    )

    # Use configure_file to copy the Doxygen file into our build directory.
    # This causes CMake to re-run if the contents of the Doxyfile change.
    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_base)

    # Read the contents of the copied Doxyfile, so that we can write modified versions
    # for XML and HTML outputs
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_base DOXYFILE_CONTENTS)

    if (DOXYGEN_FAIL_ON_WARNINGS) 
            string(JOIN "\n" DOXYFILE_CONTENTS
                   ${DOXYFILE_CONTENTS}
                   "WARN_AS_ERROR=FAIL_ON_WARNINGS_PRINT")
    endif()

    # Doxygen XML outputs
    # TODO <ndash> replacement?
    string(JOIN "\n" DOXYFILE_XML_CONTENTS
            ${DOXYFILE_CONTENTS}
            "GENERATE_HTML=NO"
            "GENERATE_XML=YES"
            "XML_OUTPUT=${SPHINX_BUILD_DIR}/xml"
            "XML_PROGRAMLISTING=NO"
            "PREDEFINED+=DOXYGEN_XML"
    )
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_xml ${DOXYFILE_XML_CONTENTS})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen_xml.stamp
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_xml ${DOXYGEN_SOURCE_FILES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_xml
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/doxygen_xml.stamp
    )

    add_custom_target(doxygen_xml
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doxygen_xml.stamp
    )

    # Doxygen HTML outputs
    string(JOIN "\n" DOXYFILE_HTML_CONTENTS
            "${DOXYFILE_CONTENTS}"
            "HTML_OUTPUT=${SPHINX_BUILD_DIR}/html_extra/doxygen"
            "INLINE_INHERITED_MEMB=YES"
    )
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_html ${DOXYFILE_HTML_CONTENTS})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen_html.stamp
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_html ${DOXYGEN_SOURCE_FILES}
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_html
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/doxygen_html.stamp
    )
    add_custom_target(doxygen_html
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doxygen_html.stamp
    )

    ####################################################################################################
    # Sphinx outputs
    ####################################################################################################

    # Sphinx HTML documentation
    add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/html.stamp"
            DEPENDS gdalapps doxygen_xml python_binding
                    ${SPHINX_SOURCE_FILES}
                    COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV} BUILDDIR=${SPHINX_BUILD_DIR}
                    ${SPHINX_BUILD} -M html
                                    ${CMAKE_CURRENT_SOURCE_DIR}/source
                                    ${SPHINX_BUILD_DIR}
                                    ${SPHINX_BUILD_OPTS}
                                    "--fail-on-warning"
            COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/html.stamp"
    )

    add_custom_target(html
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html.stamp
    )

    # Sphinx PDF documentation
    set(GDAL_DOC_PDF ${CMAKE_CURRENT_BINARY_DIR}/build/latex/gdal.pdf)
    add_custom_command(
            OUTPUT ${GDAL_DOC_PDF}
            DEPENDS gdalapps doxygen_xml ${SPHINX_SOURCE_FILES}
            COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV} BUILDDIR=${SPHINX_BUILD_DIR}
                    ${SPHINX_BUILD} -M latexpdf
                                    ${CMAKE_CURRENT_SOURCE_DIR}/source
                                    ${CMAKE_CURRENT_BINARY_DIR}/build
    )

    add_custom_target(latexpdf DEPENDS ${GDAL_DOC_PDF})

    # Sphinx manpage documentation
    add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/man.stamp"
            DEPENDS gdalapps doxygen_xml ${SPHINX_SOURCE_FILES}
            COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV} BUILDDIR=${SPHINX_BUILD_DIR}
                    ${SPHINX_BUILD} -M man
                                    ${CMAKE_CURRENT_SOURCE_DIR}/source
                                    ${CMAKE_CURRENT_BINARY_DIR}/build
                                    ${SPHINX_BUILD_OPTS}
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/build/man/_static"
            COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/man.stamp"
    )

    add_custom_target(man DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/man.stamp)

    ####################################################################################################
    # Packaged documentation (e.g. "gdal3100doc.zip")
    ####################################################################################################

    string(REPLACE "." "" _GDAL_VERSION_NO_DOTS ${GDAL_VERSION_NO_DEV_SUFFIX})
    set(DOC_ZIP "gdal${_GDAL_VERSION_NO_DOTS}doc.zip")

    # The Javadoc path is copied from swig/java/CMakeLists.txt, which hasn't been read
    # at the time this is executed.
    set(JAVADOC_ZIP ${CMAKE_CURRENT_BINARY_DIR}/../swig/java/javadoc.zip)

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${DOC_ZIP}
            DEPENDS ${GDAL_DOC_PDF}
                    ${CMAKE_CURRENT_BINARY_DIR}/html.stamp
                    java_binding ${JAVADOC_ZIP}
            COMMAND ${CMAKE_COMMAND} -E make_directory gdaldoc
            COMMAND ${CMAKE_COMMAND} -E make_directory gdaldoc/java
            COMMAND ${CMAKE_COMMAND} -E chdir gdaldoc/java ${CMAKE_COMMAND} -E tar x ${JAVADOC_ZIP} --format=zip
            COMMAND ${CMAKE_COMMAND} -E copy ${GDAL_DOC_PDF} gdaldoc/gdal.pdf
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SPHINX_BUILD_DIR}/html gdaldoc
            COMMAND ${CMAKE_COMMAND} -E tar c ${DOC_ZIP} --format=zip gdaldoc)

    add_custom_target(doczip DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${DOC_ZIP})

    ####################################################################################################
    # Documentation tests
    ####################################################################################################

    # Spell check
    add_test(NAME doc-spelling
             COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV} BUILDDIR=${SPHINX_BUILD_DIR}
                     ${SPHINX_BUILD} -b spelling
                                     ${CMAKE_CURRENT_SOURCE_DIR}/source
                                     ${CMAKE_CURRENT_BINARY_DIR}/build
                                     ${SPHINX_BUILD_OPTS}
                                     "--fail-on-warning"
                                     -D html_extra_path=extra_path
    )

    add_test(NAME doc-examples
             COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV}
             ${Python_EXECUTABLE} -m pytest -v --doctest-modules
               ${CMAKE_BINARY_DIR}/swig/python/osgeo/gdal.py
               ${CMAKE_BINARY_DIR}/swig/python/osgeo/gdal_array.py
               ${CMAKE_BINARY_DIR}/swig/python/osgeo/ogr.py
               ${CMAKE_BINARY_DIR}/swig/python/osgeo/osr.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Check docstub annotations
    add_test(NAME docstub-annotations
             COMMAND ${CMAKE_COMMAND} -E env ${BUILD_RUN_ENV}
             ${Python_EXECUTABLE} -m docstub run ${CMAKE_BINARY_DIR}/swig/python/osgeo
             --config ${CMAKE_BINARY_DIR}/swig/python/pyproject.toml --quiet
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

endif ()
