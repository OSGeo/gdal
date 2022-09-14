add_gdal_driver(
  TARGET gdal_PDF
  SOURCES gdal_pdf.h
          pdfcreatecopy.h
          pdfio.h
          pdfobject.h
          pdfcreatecopy.h
          pdfsdk_headers.h
          ogrpdflayer.cpp
          pdfcreatecopy.cpp
          pdfdataset.cpp
          pdfio.cpp
          pdfobject.cpp
          pdfreadvectors.cpp
          ogrpdflayer.cpp
          pdfwritabledataset.cpp
          pdfcreatefromcomposition.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(gdal_PDF)
target_include_directories(gdal_PDF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem
                                            ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mem)

if (GDAL_USE_POPPLER)
  target_include_directories(gdal_PDF PRIVATE ${Poppler_INCLUDE_DIRS} ${Poppler_INCLUDE_DIRS}/../)
  gdal_target_link_libraries(gdal_PDF PRIVATE Poppler::Poppler)
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\1" Poppler_VERSION_MAJOR ${Poppler_VERSION_STRING})
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\2" Poppler_VERSION_MINOR ${Poppler_VERSION_STRING})
  # Strip leading zero
  if ("${Poppler_VERSION_MINOR}" MATCHES "0?[0-9]+")
    string(REGEX REPLACE "0?([0-9]+)" "\\1" Poppler_VERSION_MINOR ${Poppler_VERSION_MINOR})
  endif ()
  if (Poppler_VERSION_MAJOR GREATER 21)
    target_compile_features(gdal_PDF PRIVATE cxx_std_17)
  endif ()
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_POPPLER -DPOPPLER_MAJOR_VERSION=${Poppler_VERSION_MAJOR}
                                              -DPOPPLER_MINOR_VERSION=${Poppler_VERSION_MINOR})

  # Workaround https://bugs.launchpad.net/ubuntu/+source/poppler/+bug/1989434
  # which is an issue with broken security update on Ubuntu 18.04 of
  # poppler 0.62.0-2ubuntu2.13
  # If we detect that gmem.h includes GooCheckedOps.h but that one is not
  # installed, create a dummy one in a temporary directory
  # To be removed once https://bugs.launchpad.net/ubuntu/+source/poppler/+bug/1989434 is fixed
  if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND
      "${Poppler_INCLUDE_DIRS}" STREQUAL "/usr/include/poppler" AND
      EXISTS /usr/include/poppler/goo/gmem.h AND
      NOT EXISTS /usr/include/poppler/goo/GooCheckedOps.h)
      file(READ /usr/include/poppler/goo/gmem.h GMEM_H_CONTENTS)
      if ("${GMEM_H_CONTENTS}" MATCHES "GooCheckedOps.h")
          set(TEMP_DIRECTORY_POPPLER "${CMAKE_CURRENT_BINARY_DIR}/temp_poppler")
          message(STATUS "Creating dummy ${TEMP_DIRECTORY_POPPLER}/GooCheckedOps.h to workaround Poppler packaging issue")
          file(MAKE_DIRECTORY "${TEMP_DIRECTORY_POPPLER}")
          file(WRITE "${TEMP_DIRECTORY_POPPLER}/GooCheckedOps.h" "// Dummy GooCheckedOps.h\n")
          target_include_directories(gdal_PDF PRIVATE "${TEMP_DIRECTORY_POPPLER}")
      endif ()
  endif ()
endif ()
if (GDAL_USE_PODOFO)
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PODOFO)
  gdal_target_link_libraries(gdal_PDF PRIVATE PODOFO::Podofo)
endif ()
if (GDAL_USE_PDFIUM)
  target_compile_features(gdal_PDF PRIVATE cxx_std_14)
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PDFIUM)
  gdal_target_link_libraries(gdal_PDF PRIVATE PDFIUM::PDFIUM)
  if (UNIX)
    # Rather hacky... Related how we build pdfium in https://github.com/rouault/pdfium_build_gdal_3_4
    gdal_target_link_libraries(
      gdal_PDF
      PRIVATE
      JPEG::JPEG
      PNG::PNG
      ${OPENJPEG_LIBRARIES}
      -lpthread
      -llcms2)
  endif ()
endif ()
