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

add_gdal_driver(TARGET ogr_OCI
                SOURCES ${SOURCE}
                CORE_SOURCES ogrocidrivercore.cpp
                PLUGIN_CAPABLE NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_OCI)
    return()
endif()

gdal_standard_includes(ogr_OCI)
gdal_target_link_libraries(ogr_OCI PRIVATE Oracle::OCI)
