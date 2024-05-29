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
  CORE_SOURCES pdfdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_PDF_core)
    if (GDAL_USE_POPPLER)
        target_compile_definitions(gdal_PDF_core PRIVATE -DHAVE_POPPLER)
    endif ()
    if (GDAL_USE_PODOFO)
        target_compile_definitions(gdal_PDF_core PRIVATE -DHAVE_PODOFO)
    endif ()
    if (GDAL_USE_PDFIUM)
        target_compile_definitions(gdal_PDF_core PRIVATE -DHAVE_PDFIUM)
    endif ()
endif()

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/pdfcomposition.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if(NOT TARGET gdal_PDF)
    return()
endif()

gdal_standard_includes(gdal_PDF)
target_include_directories(gdal_PDF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem
                                            ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mem)

if (GDAL_USE_POPPLER)
  gdal_target_link_libraries(gdal_PDF PRIVATE Poppler::Poppler)
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\1" Poppler_VERSION_MAJOR ${Poppler_VERSION_STRING})
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\2" Poppler_VERSION_MINOR ${Poppler_VERSION_STRING})
  # Strip leading zero
  if ("${Poppler_VERSION_MINOR}" MATCHES "0?[0-9]+")
    string(REGEX REPLACE "0?([0-9]+)" "\\1" Poppler_VERSION_MINOR ${Poppler_VERSION_MINOR})
  endif ()
  # POPPLER_24_05_OR_LATER used transiently (by fuzzers/build.sh) until 24.05 is actually released
  if (Poppler_VERSION_STRING VERSION_GREATER_EQUAL "24.05" OR POPPLER_24_05_OR_LATER)
    target_compile_features(gdal_PDF PRIVATE cxx_std_20)
  endif ()
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_POPPLER -DPOPPLER_MAJOR_VERSION=${Poppler_VERSION_MAJOR}
                                              -DPOPPLER_MINOR_VERSION=${Poppler_VERSION_MINOR})
endif ()
if (GDAL_USE_PODOFO)
  if( "${PODOFO_VERSION_STRING}" VERSION_GREATER "0.10")
    target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PODOFO_0_10_OR_LATER)
  endif()
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PODOFO)
  gdal_target_link_libraries(gdal_PDF PRIVATE PODOFO::Podofo)
endif ()
if (GDAL_USE_PDFIUM)
  target_compile_definitions(gdal_PDF PRIVATE -DHAVE_PDFIUM)
  gdal_target_link_libraries(gdal_PDF PRIVATE PDFIUM::PDFIUM)
  if (UNIX)
    find_library(LCMS2_LIBRARY NAMES lcms2)

    # Rather hacky... Related how we build pdfium in https://github.com/rouault/pdfium_build_gdal_3_4
    gdal_target_link_libraries(
      gdal_PDF
      PRIVATE
      JPEG::JPEG
      PNG::PNG
      ${OPENJPEG_LIBRARIES}
      Threads::Threads
      ${LCMS2_LIBRARY})
  else ()
      gdal_target_link_libraries(gdal_PDF PRIVATE winmm.lib)
  endif ()
endif ()
