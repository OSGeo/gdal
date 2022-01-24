# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

# Documentation rules
find_package(Doxygen)
if (DOXYGEN_FOUND)
  set(TARGET_HTML_DIR ${CMAKE_BINARY_DIR}/html)
  file(MAKE_DIRECTORY ${TARGET_HTML_DIR})
  # Generate translated docs. Should go first, because index.html page should be overwritten with the main one later
  add_custom_command(
    OUTPUT ${TARGET_HTML_DIR}/header.html ${TARGET_HTML_DIR}/footer.html ${TARGET_HTML_DIR}/stylesheet.css
    COMMAND ${DOXYGEN_EXECUTABLE} -w html header.html footer.html stylesheet.css
    WORKING_DIRECTORY ${TARGET_HTML_DIR})
  # Generate Russian document
  set(GDAL_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(GDAL_DOXYGEN_PROJECT_NAME "RU")
  set(GDAL_DOXYGEN_LANGUAGE "Russian")
  set(GDAL_DOXYGEN_JAVADOC_AUTOBRIEF "NO")
  set(GDAL_DOXYGEN_EXTRACT_LOCAL_CLASSES "YES")
  set(GDAL_DOXYGEN_INPUT "gdal_building_ru.dox gdal_datamodel_ru.dox gdal_tutorial_ru.dox index_ru.dox")
  set(GDAL_DOXYGEN_EXAMPLE_PATH "")
  set(GDAL_DOXYGEN_IMAGE_PATH "")
  set(GDAL_DOXYGEN_LAYOUT_FILE "")
  set(GDAL_ENABLED_SECTIONS "")
  set(GDAL_GENERATE_HTML "YES")
  set(GDAL_FILE_PATTERNS
      "*.h \
                           *.cpp \
                           *.c \
                           *.dox")
  set(GDAL_GENERATE_MAN "NO")
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_ru)
  add_custom_command(
    OUTPUT ${TARGET_HTML_DIR}/index_ru.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_ru
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_ru
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ru)
  add_custom_command(
    OUTPUT ${TARGET_HTML_DIR}/header_ru.html
    COMMAND
      ${CMAKE_COMMAND} ARGS -Dinfile:FILEPATH=${TARGET_HTML_DIR}/header.html
      -Doutfile:FILEPATH=${TARGET_HTML_DIR}/header_ru.html -Dfrom:STRING="iso-8859-1" -Dto:STRING="utf-8" -P
      ${CMAKE_CURRENT_SOURCE_DIR}/ReplaceStr.cmake
    MAIN_DEPENDENCY ${TARGET_HTML_DIR}/header.html
    WORKING_DIRECTORY ${TARGET_HTML_DIR})
  # generate Brazilian document
  set(GDAL_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(GDAL_DOXYGEN_PROJECT_NAME "BR")
  set(GDAL_DOXYGEN_LANGUAGE "Brazilian")
  set(GDAL_DOXYGEN_JAVADOC_AUTOBRIEF "NO")
  set(GDAL_DOXYGEN_EXTRACT_LOCAL_CLASSES "YES")
  set(GDAL_DOXYGEN_INPUT "gdal_building_br.dox gdal_datamodel_br.dox gdal_tutorial_br.dox index_br.dox")
  set(GDAL_DOXYGEN_EXAMPLE_PATH "")
  set(GDAL_DOXYGEN_IMAGE_PATH "")
  set(GDAL_ENABLED_SECTIONS "")
  set(GDAL_DOXYGEN_LAYOUT_FILE "")
  set(GDAL_GENERATE_HTML "YES")
  set(GDAL_FILE_PATTERNS "*.h *.cpp *.c *.dox")
  set(GDAL_GENERATE_MAN "NO")
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_br)
  add_custom_command(
    OUTPUT ${TARGET_HTML_DIR}/index_br.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_br
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_br
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/br)
  add_custom_command(
    OUTPUT ${TARGET_HTML_DIR}/header_br.html
    COMMAND ${CMAKE_COMMAND} -E copy ${TARGET_HTML_DIR}/header.html ${TARGET_HTML_DIR}/header_br.html
    DEPENDS ${TARGET_HTML_DIR}/header.html)

  # run main doxygen
  set(GDAL_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(GDAL_DOXYGEN_PROJECT_NAME "GDAL")
  set(GDAL_DOXYGEN_LANGUAGE "English")
  set(GDAL_DOXYGEN_JAVADOC_AUTOBRIEF "YES")
  set(GDAL_DOXYGEN_EXTRACT_LOCAL_CLASSES "NO")
  set(GDAL_ENABLED_SECTIONS "")
  set(GDAL_GENERATE_HTML "YES")
  set(GDAL_DOXYGEN_INCLUDE_PATH "${CMAKE_BINARY_DIR}/port ${CMAKE_BINARY_DIR}/gcore")
  set(GDAL_DOXYGEN_INPUT
      "port gcore frmts/gdalallregister.cpp alg frmts/vrt \
               doc apps ogr ogr/ogrsf_frmts ogr/ogrsf_frmts/generic \
               ogr/ogrsf_frmts/geojson/ogrgeojsonwriter.cpp \
               ogr/ogrsf_frmts/kml/ogr2kmlgeometry.cpp \
               swig/python/scripts gnm")
  set(GDAL_DOXYGEN_EXAMPLE_PATH "apps frmts frmts/jdem")
  set(GDAL_DOXYGEN_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/grid)
  set(GDAL_DOXYGEN_LAYOUT_FILE ${CMAKE_SOURCE_DIR}/DoxygenLayout.xml)
  set(GDAL_GENERATE_MAN "NO")
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_main)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_main
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_main
    ${TARGET_HTML_DIR}/header.html ${TARGET_HTML_DIR}/index_ru.html ${TARGET_HTML_DIR}/index_br.html
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  # copy rest of files for html generation
  file(
    GLOB
    OTHER_DOCS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/act-logo.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/cadcorp_logo.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/images/foss4g2013.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/foss4g2014.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/foss4g2017.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/i3-logo.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/images/ingres-logo.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/OSGeo_project.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/safe-logo.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/src-logo.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/waypoint_logo.png
    ${CMAKE_CURRENT_SOURCE_DIR}/grid/*.png
    ${CMAKE_SOURCE_DIR}/frmts/*.html
    ${CMAKE_SOURCE_DIR}/frmts/*/frmt_*.html
    ${CMAKE_SOURCE_DIR}/frmts/openjpeg/*.xml
    ${CMAKE_SOURCE_DIR}/frmts/wms/frmt_*.xml
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/*/frmt_*.html
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/*/dev_*.html
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/gpkg/geopackage_aspatial.html
    ${CMAKE_SOURCE_DIR}/ogr/*.gif
    ${CMAKE_SOURCE_DIR}/data/gdalicon.png
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/ogr_formats.html
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/ogr_formats.html
    ${CMAKE_SOURCE_DIR}/ogr/ogr_feature_style.html)
  add_custom_target(_copy_other_docs) # FIXME: are there any way without internal custom target?
  foreach (f ${OTHER_DOCS})
    add_custom_command(
      TARGET _copy_other_docs
      PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${f} ${CMAKE_BINARY_DIR}/html)
  endforeach ()
  add_custom_target(
    documents
    DEPENDS ${CMAKE_BINARY_DIR}/html/index.html
            ${CMAKE_BINARY_DIR}/html/index_ru.html
            ${CMAKE_BINARY_DIR}/html/index_br.html
            ${CMAKE_BINARY_DIR}/html/header.html
            ${CMAKE_BINARY_DIR}/html/footer.html
            ${CMAKE_BINARY_DIR}/html/stylesheet.css
            ${CMAKE_BINARY_DIR}/html/header_br.html
            ${CMAKE_BINARY_DIR}/html/header_ru.html
            _copy_other_docs)

  # Build man
  set(GDAL_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(GDAL_DOXYGEN_PROJECT_NAME "GDAL")
  set(GDAL_DOXYGEN_LANGUAGE "English")
  set(GDAL_DOXYGEN_JAVADOC_AUTOBRIEF "YES")
  set(GDAL_ENABLED_SECTIONS "man")
  set(GDAL_DOXYGEN_EXTRACT_LOCAL_CLASSES "NO")
  set(GDAL_DOXYGEN_INPUT "apps swig/python/scripts")
  set(GDAL_FILE_PATTERNS "*.cpp *.dox")
  set(GDAL_GENERATE_HTML "NO")
  set(GDAL_DOXYGEN_EXAMPLE_PATH
      "apps \
             frmts \
             frmts/jdem")
  set(GDAL_DOXYGEN_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/grid)
  set(GDAL_DOXYGEN_LAYOUT_FILE "/dev/null")
  set(GDAL_GENERATE_MAN "YES")
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_man)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/man/man1/gdalinfo.1 ${CMAKE_BINARY_DIR}/man/man1/gdal-config.1
           ${CMAKE_BINARY_DIR}/man/man1/ogrlinfo.1 ${CMAKE_BINARY_DIR}/man/man1/ogr2ogr.1
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_man
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_man
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_custom_target(manpages DEPENDS ${CMAKE_BINARY_DIR}/man/man1/gdalinfo.1 ${CMAKE_BINARY_DIR}/man/man1/gdal-config.1
                                     ${CMAKE_BINARY_DIR}/man/man1/ogrlinfo.1 ${CMAKE_BINARY_DIR}/man/man1/ogr2ogr.1)
endif ()
