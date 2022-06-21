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

# Obligatory for building GDAL; always compile in.
ogr_default_driver(mem "Read/write driver for MEMORY virtual files")
ogr_default_driver(geojson "GeoJSON/ESRIJSON/TopoJSON driver")
ogr_default_driver2(mitab TAB "MapInfo TAB and MIF/MID")
ogr_default_driver(shape "ESRI shape-file")

# ######################################################################################################################
ogr_default_driver(kml KML) # when not found EXPAT, it use CPL_MINIXML (ex. android)
ogr_default_driver(vrt "VRT - Virtual Format")

# Caution: if modifying AVC declaration here, also modify it in gdal.cmake
ogr_optional_driver(avc AVC)

ogr_optional_driver(sdts SDTS) # depends ISO8211
ogr_optional_driver(gml GML) # when not found both EXPAT/XercesC, return error in driver, referenced by WCS

# ######################################################################################################################
ogr_optional_driver(csv CSV)
ogr_optional_driver(dgn DGN)
ogr_optional_driver(gmt GMT)
ogr_optional_driver(ntf NTF)
ogr_optional_driver(s57 S57)
ogr_optional_driver(tiger "U.S. Census TIGER/Line")
ogr_optional_driver(geoconcept GEOCONCEPT)
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

# ######################################################################################################################
#
# XML drivers
ogr_dependent_driver(gpx "GPX - GPS Exchange Format" "GDAL_USE_EXPAT")
ogr_dependent_driver(gmlas GMLAS "GDAL_USE_XERCESC;OGR_ENABLE_DRIVER_PGDUMP")
ogr_dependent_driver(svg "Scalable Vector Graphics" "GDAL_USE_EXPAT")
ogr_dependent_driver(csw CSW "GDAL_USE_CURL")
ogr_dependent_driver(dwg DWG "GDAL_USE_TEIGHA")
ogr_dependent_driver(filegdb FileGDB "GDAL_USE_FILEGDB")
ogr_dependent_driver(libkml LibKML "GDAL_USE_LIBKML")
ogr_dependent_driver(nas "NAS/ALKIS" "GDAL_USE_XERCESC")
ogr_dependent_driver(plscenes PLSCENES "GDAL_USE_CURL")
ogr_dependent_driver(sosi "SOSI:Systematic Organization of Spatial Information" "GDAL_USE_FYBA")
ogr_dependent_driver(wfs "OGC WFS service" "GDAL_USE_CURL")
ogr_dependent_driver(ngw "NextGIS Web" "GDAL_USE_CURL")
ogr_dependent_driver(elastic "ElasticSearch" "GDAL_USE_CURL")

ogr_optional_driver(idrisi IDRISI)

ogr_dependent_driver(pds "Planetary Data Systems TABLE" "GDAL_ENABLE_DRIVER_PDS")

# Caution: if modifying SQLite declaration here, also modify it in gdal.cmake
ogr_dependent_driver(sqlite "SQLite3 / Spatialite RDBMS" "GDAL_USE_SQLITE3")
# Caution: if modifying GPKG declaration here, also modify it in gdal.cmake
ogr_dependent_driver(gpkg "GeoPackage" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE")
ogr_dependent_driver(osm "OpenStreetMap XML and PBF" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE")
ogr_dependent_driver(vfk "Czech Cadastral Exchange Data Format" "GDAL_USE_SQLITE3")
ogr_dependent_driver(mvt "MVT" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_OSM")

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
ogr_dependent_driver(parquet "Parquet" "GDAL_USE_PARQUET")
ogr_dependent_driver(arrow "Arrow" "GDAL_USE_ARROW")

# ######################################################################################################################
# proprietary libraries
ogr_dependent_driver(oci "Oracle OCI" "GDAL_USE_ORACLE")
ogr_dependent_driver(idb "IDB" "GDAL_USE_IDB")
ogr_dependent_driver(ods ODS "GDAL_USE_EXPAT")
ogr_dependent_driver(ogdi "OGDI" "GDAL_USE_OGDI")
ogr_dependent_driver(lvbag "LVBAG" "GDAL_USE_EXPAT")
ogr_dependent_driver(hana "SAP HANA" "GDAL_USE_ODBCCPP;GDAL_USE_ODBC")

# Add generic in last line
add_subdirectory(generic)
