include(GdalDriverHelper)

# ######################################################################################################################
# register all drivers
add_library(ogrsf_frmts OBJECT generic/ogrregisterall.cpp)
gdal_standard_includes(ogrsf_frmts)
target_include_directories(ogrsf_frmts PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generic>)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:ogrsf_frmts>)
set_property(TARGET ogrsf_frmts PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
add_dependencies(ogrsf_frmts generate_gdal_version_h)

# Exception to the rule: enable the GeoJSON driver by default, even if
# OGR_BUILD_OPTIONAL_DRIVERS=OFF.
if (NOT DEFINED OGR_ENABLE_DRIVER_GEOJSON AND
    DEFINED OGR_BUILD_OPTIONAL_DRIVERS AND
    NOT OGR_BUILD_OPTIONAL_DRIVERS)
    message(WARNING "Enabling OGR_ENABLE_DRIVER_GEOJSON=ON, despite OGR_BUILD_OPTIONAL_DRIVERS=OFF. You can of course override this choice by setting OGR_ENABLE_DRIVER_GEOJSON=OFF")
    option(OGR_ENABLE_DRIVER_GEOJSON "Set ON to build GeoJSON/GeoJSONSeq/ESRIJSON/TopoJSON drivers" ON)
endif()
ogr_optional_driver(geojson "GeoJSON/GeoJSONSeq/ESRIJSON/TopoJSON drivers")

add_feature_info(ogr_TAB OGR_ENABLE_DRIVER_TAB "MapInfo TAB and MIF/MID")
if (OGR_ENABLE_DRIVER_TAB)
    add_subdirectory(mitab)
endif ()

# Exception to the rule: enable the Shapefile driver by default, even if
# OGR_BUILD_OPTIONAL_DRIVERS=OFF.
if (NOT DEFINED OGR_ENABLE_DRIVER_SHAPE AND
    DEFINED OGR_BUILD_OPTIONAL_DRIVERS AND
    NOT OGR_BUILD_OPTIONAL_DRIVERS)
    message(WARNING "Enabling OGR_ENABLE_DRIVER_SHAPE=ON, despite OGR_BUILD_OPTIONAL_DRIVERS=OFF. You can of course override this choice by setting OGR_ENABLE_DRIVER_SHAPE=OFF")
    option(OGR_ENABLE_DRIVER_SHAPE "Set ON to build Shapefile driver" ON)
endif()
ogr_optional_driver(shape "ESRI shape-file")

# ######################################################################################################################
ogr_optional_driver(kml KML) # when not found EXPAT, it use CPL_MINIXML (ex. android)
ogr_optional_driver(vrt "VRT - Virtual Format")

# Caution: if modifying AVC declaration here, also modify it in gdal.cmake
ogr_optional_driver(avc AVC)

if (NOT(DEFINED OGR_ENABLE_DRIVER_GML AND NOT OGR_ENABLE_DRIVER_GML AND
        ((NOT GDAL_USE_CURL) OR
         (DEFINED OGR_ENABLE_DRIVER_WFS AND NOT OGR_ENABLE_DRIVER_WFS AND
          DEFINED OGR_ENABLE_DRIVER_OAPIF AND NOT OGR_ENABLE_DRIVER_OAPIF AND
          DEFINED OGR_ENABLE_DRIVER_CSW AND NOT OGR_ENABLE_DRIVER_CSW))))
    # Only build gmlutils if we have GML, WFS, OAPIF or CSW
    add_subdirectory(gmlutils)
endif()

if (GDAL_ENABLE_PLUGINS_NO_DEPS AND NOT DEFINED OGR_ENABLE_DRIVER_GML_PLUGIN)
    option(OGR_ENABLE_DRIVER_GML_PLUGIN "Set ON to build GML driver as a plugin" OFF)
endif ()
ogr_optional_driver(gml GML)
if (OGR_ENABLE_DRIVER_GML_PLUGIN AND
    ((NOT DEFINED OGR_ENABLE_DRIVER_NAS OR OGR_ENABLE_DRIVER_NAS)))
    message(FATAL_ERROR "When building the GML driver as a plugin, the NAS driver must be explicitly disabled with OGR_ENABLE_DRIVER_NAS=OFF")
endif()

# ######################################################################################################################
ogr_optional_driver(csv CSV)
ogr_optional_driver(dgn DGN)
ogr_optional_driver(gmt GMT)
ogr_optional_driver(s57 S57)
ogr_optional_driver(georss GEORSS)
ogr_optional_driver(dxf DXF)
ogr_optional_driver(pgdump PGDump)
ogr_optional_driver(gpsbabel GPSBABEL)
ogr_optional_driver(edigeo EDIGEO)
ogr_optional_driver(sxf SXF)
ogr_optional_driver(openfilegdb OPENFILEGDB)
ogr_optional_driver(wasp "WAsP .map format")
ogr_optional_driver(selafin OSELAFIN)
ogr_optional_driver(jml JML)
ogr_optional_driver(vdv "VDV-451/VDV-452/INTREST Data Format")
ogr_optional_driver(flatgeobuf FlatGeobuf)
ogr_optional_driver(mapml MapML)
ogr_optional_driver(adbc ADBC)

if( NOT WORDS_BIGENDIAN )
    ogr_optional_driver(miramon "MiraMonVector")
endif()

ogr_optional_driver(aivector AIVector)

# ######################################################################################################################
#
if (NOT OGR_ENABLE_DRIVER_GEOJSON_PLUGIN)
    ogr_dependent_driver(jsonfg JSONFG "OGR_ENABLE_DRIVER_GEOJSON")
endif()

# XML drivers
ogr_dependent_driver(gpx "GPX - GPS Exchange Format" "GDAL_USE_EXPAT")
ogr_dependent_driver(gmlas GMLAS "GDAL_USE_XERCESC;OGR_ENABLE_DRIVER_PGDUMP")
ogr_dependent_driver(csw CSW "GDAL_USE_CURL")
ogr_dependent_driver(dwg DWG "GDAL_USE_TEIGHA")
ogr_dependent_driver(filegdb FileGDB "GDAL_USE_FILEGDB")
ogr_dependent_driver(libkml LibKML "GDAL_USE_LIBKML")
ogr_dependent_driver(nas "NAS/ALKIS" "GDAL_USE_XERCESC;OGR_ENABLE_DRIVER_GML")
ogr_dependent_driver(plscenes PLSCENES "GDAL_USE_CURL")
ogr_dependent_driver(sosi "SOSI:Systematic Organization of Spatial Information" "GDAL_USE_FYBA")
ogr_dependent_driver(wfs "OGC WFS service" "GDAL_USE_CURL")
ogr_dependent_driver(oapif "OGC API Features service" "GDAL_USE_CURL")
ogr_dependent_driver(ngw "NextGIS Web" "GDAL_USE_CURL")
ogr_dependent_driver(elastic "ElasticSearch" "GDAL_USE_CURL")
ogr_dependent_driver(xodr OpenDRIVE "GDAL_USE_OPENDRIVE;GDAL_USE_GEOS")

ogr_dependent_driver(idrisi IDRISI "GDAL_ENABLE_DRIVER_IDRISI")

ogr_dependent_driver(pds "Planetary Data Systems TABLE" "GDAL_ENABLE_DRIVER_PDS")

# Caution: if modifying SQLite declaration here, also modify it in gdal.cmake
ogr_dependent_driver(sqlite "SQLite3 / Spatialite RDBMS" "GDAL_USE_SQLITE3")
# Caution: if modifying GPKG declaration here, also modify it in gdal.cmake
ogr_dependent_driver(gpkg "GeoPackage" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE")
ogr_dependent_driver(osm "OpenStreetMap XML and PBF" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE")
ogr_dependent_driver(vfk "Czech Cadastral Exchange Data Format" "GDAL_USE_SQLITE3")
# Caution: if modifying MVT declaration here, also modify it in gdal.cmake
ogr_dependent_driver(mvt "MVT" "GDAL_USE_SQLITE3")
ogr_dependent_driver(pmtiles PMTiles "OGR_ENABLE_DRIVER_MVT")

# ODBC/POSTGRES/MYSQL
ogr_dependent_driver(amigocloud AMIGOCLOUD "GDAL_USE_CURL;OGR_ENABLE_DRIVER_PGDUMP")
ogr_dependent_driver(carto CARTO "GDAL_USE_CURL;OGR_ENABLE_DRIVER_PGDUMP")
ogr_dependent_driver(ili ILI "GDAL_USE_XERCESC")
ogr_dependent_driver(mysql MySQL "GDAL_USE_MYSQL")
ogr_dependent_driver(pg PostGIS "GDAL_USE_POSTGRESQL;OGR_ENABLE_DRIVER_PGDUMP")
ogr_dependent_driver(mssqlspatial MSSQLSPATIAL "GDAL_USE_ODBC")
ogr_dependent_driver(odbc ODBC "GDAL_USE_ODBC")
ogr_dependent_driver(pgeo "PGEO" "GDAL_USE_ODBC")

ogr_dependent_driver(xlsx "Microsoft Office Excel(xlsx)" "GDAL_USE_EXPAT")
ogr_dependent_driver(xls "Microsoft Office Excel(xls)" "GDAL_USE_FREEXL")
ogr_dependent_driver(mongodbv3 "MongoDB V3" "GDAL_USE_MONGOCXX")
ogr_dependent_driver(cad "OpenCAD" "GDAL_USE_OPENCAD OR GDAL_USE_OPENCAD_INTERNAL")
include(parquet/driver_declaration.cmake)
include(arrow/driver_declaration.cmake)

ogr_dependent_driver(gtfs "GTFS" "OGR_ENABLE_DRIVER_CSV")

# ######################################################################################################################
# proprietary libraries
include(oci/driver_declaration.cmake)
ogr_dependent_driver(idb "IDB" "GDAL_USE_IDB")
ogr_dependent_driver(ods ODS "GDAL_USE_EXPAT")
ogr_dependent_driver(lvbag "LVBAG" "GDAL_USE_EXPAT")
ogr_dependent_driver(hana "SAP HANA" "GDAL_USE_ODBCCPP;GDAL_USE_ODBC")

# Add generic in last line
add_subdirectory(generic)
