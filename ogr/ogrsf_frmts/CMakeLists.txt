include(GdalDriverHelper)

# ######################################################################################################################
# register all drivers
add_library(ogrsf_frmts OBJECT generic/ogrregisterall.cpp)
gdal_standard_includes(ogrsf_frmts)
target_include_directories(ogrsf_frmts PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generic>)
target_sources(gdal PRIVATE $<TARGET_OBJECTS:ogrsf_frmts>)
set_property(TARGET ogrsf_frmts PROPERTY POSITION_INDEPENDENT_CODE ON)

# Obligatory for building GDAL; always compile in.
ogr_default_driver(mem "Read/write driver for MEMORY virtual files")
ogr_default_driver(geojson "GeoJSON/ESRIJSON/TopoJSON driver")
ogr_default_driver2(mitab TAB "MapInfo TAB and MIF/MID")

# ######################################################################################################################
ogr_default_driver(kml KML) # when not found EXPAT, it use CPL_MINIXML (ex. android)
ogr_default_driver(vrt "VRT - Virtual Format")
ogr_default_driver(sdts SDTS) # depends ISO8211
ogr_default_driver(gml GML) # when not found both EXPAT/XercesC, return error in driver, referenced by WCS

# ######################################################################################################################
ogr_optional_driver(avc AVC)
ogr_optional_driver(csv CSV)
ogr_optional_driver(dgn DGN)
ogr_optional_driver(gmt GMT)
ogr_optional_driver(ntf NTF)
ogr_optional_driver(rec REC)
option(ENABLE_S57_BUILTIN_CLASSES "Set ON to build s57 driver with builin classes" OFF)
ogr_optional_driver(s57 S57)
ogr_optional_driver(shape "ESRI shape-file")
ogr_optional_driver(tiger "U.S. Census TIGER/Line")
ogr_optional_driver(geoconcept GEOCONCEPT)
ogr_optional_driver(georss GEORSS)
ogr_optional_driver(gtm "GPS TrackMaker")
ogr_optional_driver(dxf DXF)
ogr_optional_driver(pgdump PGDump)
ogr_optional_driver(gpsbabel GPSBABEL)
ogr_optional_driver(edigeo EDIGEO)
ogr_optional_driver(arcgen ARCGEN)
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
ogr_dependent_driver(gpx "GPX - GPS Exchange Format" "HAVE_EXPAT")
ogr_dependent_driver(gmlas GMLAS "HAVE_XERCESC;HAVE_JSONC;OGR_ENABLE_PGDUMP")
ogr_dependent_driver(svg "Scalable Vector Graphics" "HAVE_EXPAT")
ogr_dependent_driver(csw CSW "CURL_FOUND")
ogr_dependent_driver(dods DODS "HAVE_DAP;HAVE_LIBXML2")
ogr_dependent_driver(dwg DWG "HAVE_TEIGHA")
ogr_dependent_driver(filegdb FileGDB "HAVE_FGDB")
# ogr_dependent_driver(gft GFT "HAVE_CURL")
ogr_dependent_driver(libkml LibKML "LIBKML_FOUND;Boost_FOUND")
ogr_dependent_driver(nas "NAS/ALKIS" "HAVE_XERCESC")
ogr_dependent_driver(plscenes PLSCENES "CURL_FOUND")
ogr_dependent_driver(sosi "SOSI:Systematic Organization of Spatial Information" "HAVE_SOCI")
ogr_dependent_driver(wfs "OGC WFS service" "CURL_FOUND")
ogr_dependent_driver(dds "DDS" "HAVE_CRNLIB")
ogr_dependent_driver(ngw "NextGIS Web" "CURL_FOUND")

ogr_optional_driver(elastic ELASTIC)
ogr_optional_driver(idrisi IDRISI)

ogr_dependent_driver(pds "Planetary Data Systems TABLE" "GDAL_ENABLE_FRMT_PDS")

ogr_dependent_driver(sqlite "SQLite3 / Spatialite RDBMS" "SQLite3_FOUND")
# GeoPacage is a part of sqlite driver. when enable sqlite, automatically add a gpkg
if (OGR_ENABLE_SQLITE)
  add_subdirectory(gpkg)
endif ()
add_feature_info(ogr_GPKG OGR_ENABLE_SQLITE "GeoPackage")
ogr_dependent_driver(osm "OpenStreetMap XML and PBF" "OGR_ENABLE_SQLITE")
ogr_dependent_driver(vfk "Czech Cadastral Exchange Data Format" "SQLite3_FOUND")
ogr_dependent_driver(mvt "MVT" "SQLite3_FOUND;OGR_ENABLE_OSM")

# ODBC/POSTGRES/MYSQL/INGRESS/COUCHDB
ogr_dependent_driver(amigocloud AMIGOCLOUD "CURL_FOUND;OGR_ENABLE_PGDUMP")
ogr_dependent_driver(cloudant Cloudant "CURL_FOUND")
ogr_dependent_driver(couchdb CouchDB "CURL_FOUND")
ogr_dependent_driver(carto CARTO "OGR_ENABLE_PGDUMP")
ogr_dependent_driver(ingress INGRESS "HAVE_INGRESS")
ogr_dependent_driver(ili ILI "HAVE_XERCESC")
ogr_dependent_driver(mysql MySQL "HAVE_MYSQL")
ogr_dependent_driver(pg PostGIS "PostgreSQL_FOUND;OGR_ENABLE_PGDUMP")
ogr_dependent_driver(geomedia GEOMEDIA "ODBC_FOUND")
ogr_dependent_driver(mssqlspatial MSSQLSPATIAL "ODBC_FOUND")
ogr_dependent_driver(odbc ODBC "ODBC_FOUND")
ogr_dependent_driver(walk "WALK Spatial data" "ODBC_FOUND")
ogr_dependent_driver(pgeo "PGEO" "ODBC_FOUND")

# for dependency problem, grass is not always compiled in.
ogr_dependent_driver(grass "GRASS Vector format" "HAVE_GRASS")
if (OGR_ENABLE_GRASS)
  if (HAVE_GRASS AND HAVE_POSTGRESQL)

  else ()
    message(FATAL_ERROR "Dependency error on GRASS driver")
  endif ()
endif ()

ogr_dependent_driver(xlsx "Microsoft Office Excel(xlsx)" "HAVE_EXPAT")
ogr_dependent_driver(xls "Microsoft Office Excel(xls)" "FREEXL_FOUND")
ogr_dependent_driver(mongodb "MongoDB" "MONGODB_FOUND")
ogr_optional_driver(cad "OpenCAD")

# ######################################################################################################################
# proprietary libraries
ogr_dependent_driver(fme "FME" "HAVE_FME")
ogr_dependent_driver(oci "Oracle OCI" "HAVE_ORACLE")
ogr_dependent_driver(db2 "IBM DB2" "HAVE_ODBC")
ogr_dependent_driver(idb "IDB" "HAVE_IDB")
# to be removed in GDAL 3.5 ogr_dependent_driver(arcobjects ARCOBJECTS "ARCOBJECTS_FOUND")
ogr_dependent_driver(ods ODS "HAVE_EXPAT")
ogr_dependent_driver(ogdi "OGDI" "HAVE_OGDI")
ogr_dependent_driver(lvbag "LVBAG" "HAVE_EXPAT")

# Add generic in last line
add_subdirectory(generic)
