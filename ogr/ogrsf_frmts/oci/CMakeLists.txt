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

add_gdal_driver(TARGET ogr_OCI SOURCES ${SOURCE} PLUGIN_CAPABLE)

gdal_standard_includes(ogr_OCI)
gdal_target_link_libraries(TARGET ogr_OCI LIBRARIES Oracle::OCI)
