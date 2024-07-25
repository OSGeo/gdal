add_gdal_driver(TARGET ogr_ODBC
                SOURCES
                    ogrodbcdatasource.cpp
                    ogrodbcdriver.cpp
                    ogrodbclayer.cpp
                    ogrodbcselectlayer.cpp
                    ogrodbctablelayer.cpp
                CORE_SOURCES
                    ogrodbcdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_ODBC)
    return()
endif()

gdal_standard_includes(ogr_ODBC)
gdal_target_link_libraries(ogr_ODBC PRIVATE ODBC::ODBC)
