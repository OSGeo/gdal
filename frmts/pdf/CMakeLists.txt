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
  target_include_directories(gdal_PDF PRIVATE ${POPPLER_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS}/../)
  gdal_target_link_libraries(TARGET gdal_PDF LIBRARIES POPPLER::Poppler)
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\1" POPPLER_VERSION_MAJOR ${POPPLER_VERSION_STRING})
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\2" POPPLER_VERSION_MINOR ${POPPLER_VERSION_STRING})
  # Strip leading zero
  if ("${POPPLER_VERSION_MINOR}" MATCHES "0?[0-9]+")
    string(REGEX REPLACE "0?([0-9]+)" "\\1" POPPLER_VERSION_MINOR ${POPPLER_VERSION_MINOR})
  endif ()
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_POPPLER -DPOPPLER_MAJOR_VERSION=${POPPLER_VERSION_MAJOR}
                                              -DPOPPLER_MINOR_VERSION=${POPPLER_VERSION_MINOR})
endif ()
if (GDAL_USE_PODOFO)
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PODOFO)
  gdal_target_link_libraries(TARGET gdal_PDF LIBRARIES PODOFO::Podofo)
endif ()
if (GDAL_USE_PDFIUM)
  target_compile_features(gdal_PDF PRIVATE cxx_std_14)
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PDFIUM)
  gdal_target_link_libraries(TARGET gdal_PDF LIBRARIES PDFIUM::PDFium)
  if (UNIX)
    # Rather hacky... Related how we build pdfium in https://github.com/rouault/pdfium_build_gdal_3_4
    gdal_target_link_libraries(TARGET gdal_PDF LIBRARIES JPEG::JPEG PNG::PNG ${OPENJPEG_LIBRARIES} -lpthread -llcms2)
  endif ()
endif ()
