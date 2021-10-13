option(OGR_ENABLE_OCI_PLUGIN "Enable plugin build for Oracle OCI" ON)
set(SOURCE
    oci_utils.cpp
    ocitest.cpp
    ogrocidatasource.cpp
    ogrocidriver.cpp
    ogrocilayer.cpp
    ogrociloaderlayer.cpp
    ogrociselectlayer.cpp
    ogrocisession.cpp
    ogrocistatement.cpp
    ogrocistringbuf.cpp
    ogrocistroke.cpp
    ogrocitablelayer.cpp
    ogrociwritablelayer.cpp)
if (OGR_ENABLE_OCI_PLUGIN)
  add_gdal_driver(
    TARGET ogr_OCI
    SOURCES ${SOURCE}
    PLUGIN)
else ()
  add_gdal_driver(TARGET ogr_OCI SOURCES ${SOURCE})
endif ()
gdal_standard_includes(ogr_OCI)
target_include_directories(ogr_OCI PRIVATE ${OCI_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET ogr_OCI LIBRARIES ${OCI_LIBRARIES})
